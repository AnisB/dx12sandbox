// Bento includes
#include <bento_base/security.h>
#include <bento_memory/common.h>
#include <bento_base/log.h>

// DX12 includes
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <dxcapi.h>

// Internal includes
#include "gpu_backend/dx12_backend.h"
#include "gpu_backend/event_collector.h"

#define DX12_NUM_BACK_BUFFERS 2

namespace graphics_sandbox
{
	LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_PAINT:
			event_collector::push_event(FrameEvent::Paint);
			break;
		case WM_CLOSE:
			event_collector::push_event(FrameEvent::Close);
			break;
		case WM_DESTROY:
			event_collector::push_event(FrameEvent::Destroy);
			break;
		default:
			return DefWindowProc(hwnd, message, wParam, lParam); // add this
		}
		return 0;
	}

	namespace d3d12
	{
		TGraphicSettings default_settings()
		{
			TGraphicSettings settings;
			settings.width = 1280;
			settings.height = 720;
			settings.fullscreen = false;
			settings.backend = RenderingBackEnd::DX12;
			return settings;
		}

		struct DX12Window
		{
			HWND window;
			uint32_t width;
			uint32_t height;
		};

		struct DX12GraphicsDevice
		{
			ID3D12Device2* device;
			ID3D12Debug* debugLayer;
			uint32_t descriptorSize[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
		};

		struct DX12CommandQueue
		{
			ID3D12CommandQueue* queue;
			ID3D12Fence* fence;
			uint64_t fenceValue;
		};

		struct DX12RenderTexture
		{
			// Actual resource
			ID3D12Resource* resource;
			D3D12_RESOURCE_STATES state;

			// Descriptor heap and offset (for the view)
			ID3D12DescriptorHeap* descriptorHeap;
			uint32_t heapOffset;
			// Tells us if the heap is owned by the rendertarget or not
			bool rtOwned;
		};

		struct DX12SwapChain
		{
			// Swap chain
			IDXGISwapChain4* swapChain;
			uint32_t currentBackBuffer;

			// Description heap required for the swap chain
			ID3D12DescriptorHeap* descriptorHeap;

			// Back Buffers
			DX12RenderTexture backBufferRenderTexture[DX12_NUM_BACK_BUFFERS];
		};

		struct DX12CommandBuffer
		{
			DX12GraphicsDevice* deviceI;
			ID3D12CommandAllocator* cmdAlloc;
			ID3D12GraphicsCommandList* cmdList;
		};

		struct DX12ComputeShader
		{
			IDxcBlob* shaderBlob;
			ID3D12RootSignature* rootSignature;
			ID3D12PipelineState* pipelineStateObject;
			ID3D12DescriptorHeap* descriptorHeap;

			// GPU Handles for every resource type
			D3D12_GPU_DESCRIPTOR_HANDLE srvGPU;
			D3D12_GPU_DESCRIPTOR_HANDLE uavGPU;
			D3D12_GPU_DESCRIPTOR_HANDLE cbvGPU;

			// CPU Handles for every resource type
			D3D12_CPU_DESCRIPTOR_HANDLE srvCPU;
			D3D12_CPU_DESCRIPTOR_HANDLE uavCPU;
			D3D12_CPU_DESCRIPTOR_HANDLE cbvCPU;

			// Number of resources
			uint32_t srvCount;
			uint32_t uavCount;
			uint32_t cbvCount;
		};

		struct DX12GraphicsBuffer
		{
			ID3D12Resource* resource;
			D3D12_RESOURCE_STATES state;
			uint64_t bufferSize;
			uint8_t elementSize;
			GraphicsBufferType type;
		};

		// Function to register the window
		void RegisterWindowClass(HINSTANCE hInst, const wchar_t* windowClassName)
		{
			// Register a window class for creating our render window with.
			WNDCLASSEXW windowClass = {};

			windowClass.cbSize = sizeof(WNDCLASSEX);
			windowClass.style = CS_HREDRAW | CS_VREDRAW;
			windowClass.lpfnWndProc = &WndProc;
			windowClass.cbClsExtra = 0;
			windowClass.cbWndExtra = 0;
			windowClass.hInstance = hInst;
			windowClass.hIcon = LoadIcon(hInst, NULL);
			windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
			windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
			windowClass.lpszMenuName = NULL;
			windowClass.lpszClassName = windowClassName;
			windowClass.hIconSm = LoadIcon(hInst, NULL);

			// Register the window
			assert_msg(RegisterClassExW(&windowClass) > 0, "RegisterClassExW failed.");
		}

		void EvaluateWindowParameters(uint32_t width, uint32_t height, uint32_t& windowWidth, uint32_t& windowHeight, uint32_t& windowX, uint32_t& windowY)
		{
			// Get the size of the monitor
			int screenWidth = GetSystemMetrics(SM_CXSCREEN);
			int screenHeight = GetSystemMetrics(SM_CYSCREEN);

			// Calculates the required size of the window rectangle, based on the desired client-rectangle size.
			RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
			assert_msg(AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE), "AdjustWindowRect failed.");

			windowWidth = windowRect.right - windowRect.left;
			windowHeight = windowRect.bottom - windowRect.top;

			// Center the window within the screen. Clamp to 0, 0 for the top-left corner.
			windowX = std::max<int>(0, (screenWidth - windowWidth) / 2);
			windowY = std::max<int>(0, (screenHeight - windowHeight) / 2);
		}

		// Function to create the window
		HWND CreateWindowInternal(const wchar_t* windowClassName, HINSTANCE hInst, const wchar_t* windowTitle, uint32_t width, uint32_t height)
		{
			// Evaluate the actual size and location of the window
			uint32_t windowWidth = 1, windowHeight = 1, windowX = 0, windowY = 0;
			EvaluateWindowParameters(width, height, windowWidth, windowHeight, windowX, windowY);

			// Center the window within the screen.
			HWND hWnd = CreateWindowExW(NULL, windowClassName, windowTitle, WS_OVERLAPPEDWINDOW, windowX, windowY, windowWidth, windowHeight, NULL, NULL, hInst, nullptr);
			assert_msg(hWnd != nullptr, "Failed to create window");

			// Return the created window
			return hWnd;
		}

		// Function to create the command queue
		ID3D12CommandQueue* CreateCommandQueue(ID3D12Device2* device, D3D12_COMMAND_LIST_TYPE type)
		{
			D3D12_COMMAND_QUEUE_DESC desc = {};
			desc.Type = type;
			desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
			desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			desc.NodeMask = 0;

			// Create the command queue and ensure it's been succesfully created
			ID3D12CommandQueue* d3d12CommandQueue;
			assert_msg(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3d12CommandQueue)) == S_OK, "Command queue creation failed.");

			// Return the command queue
			return d3d12CommandQueue;
		}

		// Function to create the swap chain
		IDXGISwapChain4* CreateSwapChain(HWND hWnd, ID3D12CommandQueue* commandQueue, uint32_t width, uint32_t height, uint32_t bufferCount)
		{
			// Grab the DXGI factory 2
			IDXGIFactory4* dxgiFactory4;
			UINT createFactoryFlags = 0;
			assert_msg(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)) == S_OK, "DXGI Factory 2 request failed.");

			// Describe the swap chain
			DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
			swapChainDesc.Width = width;
			swapChainDesc.Height = height;
			swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			swapChainDesc.Stereo = FALSE;
			swapChainDesc.SampleDesc = { 1, 0 };
			swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapChainDesc.BufferCount = bufferCount;
			swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
			swapChainDesc.Flags = 0;

			// Create the swapchain
			IDXGISwapChain1* swapChain1;
			assert_msg(dxgiFactory4->CreateSwapChainForHwnd(commandQueue, hWnd, &swapChainDesc, nullptr, nullptr, &swapChain1) == S_OK, "Create Swap Chain failed.");

			// Cnonvert to the Swap Chain 4 structure
			IDXGISwapChain4* dxgiSwapChain4 = (IDXGISwapChain4*)swapChain1;

			// Release the resources
			dxgiFactory4->Release();

			// Return the swap chain
			return dxgiSwapChain4;
		}

		ID3D12DescriptorHeap* create_descriptor_heap(ID3D12Device2* device, uint32_t numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type)
		{
			// Describe and create a render target view (RTV) descriptor heap.
			D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
			rtvHeapDesc.NumDescriptors = numDescriptors;
			rtvHeapDesc.Type = type;
			if (type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV || type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
				rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			else
				rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			ID3D12DescriptorHeap* descriptorHeap;
			assert_msg(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&descriptorHeap)) == S_OK, "Failed to create descriptor heap.");
			return descriptorHeap;
		}

		std::wstring convert_to_wide(const std::string& str)
		{
			size_t stringSize = str.size();
			std::wstring wc(stringSize, L'#');
			mbstowcs(&wc[0], str.c_str(), stringSize);
			return wc;
		}

		std::wstring convert_to_wide(const char* str, uint32_t strLength)
		{
			size_t stringSize = strLength;
			std::wstring wc(stringSize, L'#');
			mbstowcs(&wc[0], str, stringSize);
			return wc;
		}

		// Window API
		namespace window
		{
			RenderWindow create_window(const TGraphicSettings& graphic_settings)
			{
				// Create the window internal structure
				DX12Window* dx12_window = bento::make_new<DX12Window>(*bento::common_allocator());

				// Convert the name from normal to wide
				std::wstring wc = convert_to_wide(graphic_settings.window_name);

				// Grab the instance
				HINSTANCE hInst = (HINSTANCE)graphic_settings.data[0];

				// Register the window
				RegisterWindowClass(hInst, wc.c_str());

				// Create the window
				dx12_window->width = graphic_settings.width;
				dx12_window->height = graphic_settings.height;
				dx12_window->window = CreateWindowInternal(wc.c_str(), hInst, wc.c_str(), dx12_window->width, dx12_window->height);
				assert_msg(dx12_window->window != nullptr, "Failed to create window.");

				// Cast the window to the opaque type
				return (RenderWindow)dx12_window;
			}

			void destroy_window(RenderWindow renderWindow)
			{
				// Grab the internal windows structure
				DX12Window* dx12_window = (DX12Window*)renderWindow;

				// Destroy the actual window
				assert_msg(DestroyWindow(dx12_window->window), "Failed to destroy window.");

				// Detroy the internal window structure
				bento::make_delete<DX12Window>(*bento::common_allocator(), dx12_window);
			}

			void show(RenderWindow renderWindow)
			{
				DX12Window* dx12_window = (DX12Window*)renderWindow;
				ShowWindow(dx12_window->window, SW_SHOWDEFAULT);
			}

			void hide(RenderWindow renderWindow)
			{
				DX12Window* dx12_window = (DX12Window*)renderWindow;
				ShowWindow(dx12_window->window, SW_HIDE);
			}
		}

		// Graphics Device API
		namespace graphics_device
		{
			// On DX12 to create a graphics device, we need to fetch the adapter of the right device.
			IDXGIAdapter4* GetAdapter()
			{
				// Create the DXGI factory
				IDXGIFactory4* dxgiFactory;
				UINT createFactoryFlags = 0;
				assert_msg(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)) == S_OK, "DXGI Factory 2 failed.");

				// Loop through all the available dapters
				IDXGIAdapter1* dxgiAdapter1;
				IDXGIAdapter4* dxgiAdapter4 = nullptr;
				SIZE_T maxDedicatedVideoMemory = 0;
				for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
				{
					DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
					dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

					// Check to see if the adapter can create a D3D12 device without actually creating it. The adapter with the largest dedicated video memory is favored.
					if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 && SUCCEEDED(D3D12CreateDevice(dxgiAdapter1, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
						dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
					{
						maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
						dxgiAdapter4 = (IDXGIAdapter4*)dxgiAdapter1;
						assert_msg(dxgiAdapter4 != nullptr, "Failed to convert DXGI Adapter from 1 to 4.");
					}
				}

				// Do not forget to release the resources
				dxgiFactory->Release();

				// Return the adapter
				return dxgiAdapter4;
			}

			GraphicsDevice create_graphics_device()
			{
				// Grab the allocator
				bento::IAllocator* allocator = bento::common_allocator();
				assert(allocator != nullptr);

				// Debug Interface
				ID3D12Debug* debugInterface = nullptr;
				// assert_msg(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)) == S_OK, "Debug layer failed to initialize");
				// debugInterface->EnableDebugLayer();

				// Grab the right adapter
				IDXGIAdapter4* adapter = GetAdapter();

				// Create the graphics device and ensure it's been succesfully created
				ID3D12Device2* d3d12Device2;
				assert_msg(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2)) == S_OK, "D3D12 Device creation failed.");

				// Do not forget to release the adapter
				adapter->Release();

				// Create the graphics device internal structure
				DX12GraphicsDevice* dx12_graphicsDevice = bento::make_new<DX12GraphicsDevice>(*allocator);
				dx12_graphicsDevice->device = d3d12Device2;
				dx12_graphicsDevice->debugLayer = debugInterface;
				for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
					dx12_graphicsDevice->descriptorSize[i] = d3d12Device2->GetDescriptorHandleIncrementSize((D3D12_DESCRIPTOR_HEAP_TYPE)i);

				return (GraphicsDevice)dx12_graphicsDevice;
			}

			void destroy_graphics_device(GraphicsDevice graphicsDevice)
			{
				DX12GraphicsDevice* dx12_device = (DX12GraphicsDevice*)graphicsDevice;
				dx12_device->device->Release();
				// dx12_device->debugLayer->Release();
			}
		}

		namespace command_queue
		{
			CommandQueue create_command_queue(GraphicsDevice graphicsDevice)
			{
				DX12GraphicsDevice* dx12_device = (DX12GraphicsDevice*)graphicsDevice;
				ID3D12CommandQueue* commandQueue = CreateCommandQueue(dx12_device->device, D3D12_COMMAND_LIST_TYPE_DIRECT);
				assert_msg(commandQueue != nullptr, "Failed to create command queue.");

				DX12CommandQueue* dx12_commandQueue = bento::make_new<DX12CommandQueue>(*bento::common_allocator());
				dx12_commandQueue->queue = commandQueue;
				dx12_commandQueue->fence = (ID3D12Fence*)fence::create_fence(graphicsDevice);
				dx12_commandQueue->fenceValue = 0;
				return (CommandQueue)dx12_commandQueue;
			}

			void destroy_command_queue(CommandQueue commandQueue)
			{
				DX12CommandQueue* dx12_commandQueue = (DX12CommandQueue*)commandQueue;
				dx12_commandQueue->queue->Release();
				fence::destroy_fence((Fence)dx12_commandQueue->fence);
				bento::make_delete<DX12CommandQueue>(*bento::common_allocator(), dx12_commandQueue);
			}

			void execute_command_buffer(CommandQueue commandQueue, CommandBuffer commandBuffer)
			{
				// Grab the internal structures
				DX12CommandBuffer* dx12_commandBuffer = (DX12CommandBuffer*)commandBuffer;
				DX12CommandQueue* dx12_commandQueue = (DX12CommandQueue*)commandQueue;

				ID3D12CommandList* const commandLists[] = { dx12_commandBuffer->cmdList};
				dx12_commandQueue->queue->ExecuteCommandLists(1, commandLists);
			}
			
			void flush(CommandQueue commandQueue)
			{
				DX12CommandQueue* dx12_commandQueue = (DX12CommandQueue*)commandQueue;
				dx12_commandQueue->queue->Signal(dx12_commandQueue->fence, dx12_commandQueue->fenceValue);
				HANDLE handle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
				dx12_commandQueue->fence->SetEventOnCompletion(dx12_commandQueue->fenceValue, handle);
				WaitForSingleObject(handle, INFINITE);
				dx12_commandQueue->fenceValue++;
			}
		}

		// Swap Chain API
		namespace swap_chain
		{
			// Creation and Destruction
			SwapChain create_swap_chain(RenderWindow renderWindow, GraphicsDevice graphicsDevice, CommandQueue commandQueue)
			{
				// Grab the actual structures
				DX12Window* dx12_window = (DX12Window*)renderWindow;
				DX12CommandQueue* dx12_commandQueue = (DX12CommandQueue*)commandQueue;
				DX12GraphicsDevice* deviceI = (DX12GraphicsDevice*)graphicsDevice;
				ID3D12Device2* device = deviceI->device;

				bento::IAllocator* allocator = bento::common_allocator();
				assert(allocator != nullptr);

				// Create the render environment internal structure
				DX12SwapChain* swapChainI = bento::make_new<DX12SwapChain>(*allocator);

				// Create the swap chain
				swapChainI->swapChain = CreateSwapChain(dx12_window->window, dx12_commandQueue->queue, dx12_window->width, dx12_window->height, DX12_NUM_BACK_BUFFERS);

				// Grab the current back buffer
				swapChainI->currentBackBuffer = swapChainI->swapChain->GetCurrentBackBufferIndex();

				// Create the descriptor heap for the swap chain
				swapChainI->descriptorHeap = create_descriptor_heap(device, DX12_NUM_BACK_BUFFERS, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

				// Start of the heap
				D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(swapChainI->descriptorHeap->GetCPUDescriptorHandleForHeapStart());

				// Create a RTV for each frame.
				for (uint32_t n = 0; n < DX12_NUM_BACK_BUFFERS; n++)
				{
					// Keep track of the descriptor heap where this is stored
					DX12RenderTexture& currentRenderTexture = swapChainI->backBufferRenderTexture[n];
					currentRenderTexture.state = D3D12_RESOURCE_STATE_PRESENT;
					currentRenderTexture.descriptorHeap = swapChainI->descriptorHeap;
					currentRenderTexture.heapOffset = deviceI->descriptorSize[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] * n;

					// Grab the buffer of the swap chain
					assert_msg(swapChainI->swapChain->GetBuffer(n, IID_PPV_ARGS(&swapChainI->backBufferRenderTexture[n].resource)) == S_OK, "Failed to get the swap chain buffer.");

					// Create a render target view for it
					device->CreateRenderTargetView(swapChainI->backBufferRenderTexture[n].resource, nullptr, rtvHandle);

					// Move on to the next pointer
					rtvHandle.ptr += (1 * deviceI->descriptorSize[D3D12_DESCRIPTOR_HEAP_TYPE_RTV]);
				}

				// Return the opaque structure
				return (SwapChain)swapChainI;
			}

			void destroy_swap_chain(SwapChain swapChain)
			{
				// Grab the actual structure
				DX12SwapChain* dx12_swapChain = (DX12SwapChain*)swapChain;

				// Release the render target views
				for (uint32_t n = 0; n < DX12_NUM_BACK_BUFFERS; n++)
					dx12_swapChain->backBufferRenderTexture[n].resource->Release();

				// Release the DX12 structures
				dx12_swapChain->descriptorHeap->Release();
				dx12_swapChain->swapChain->Release();

				// Release the internal structure
				bento::make_delete<DX12SwapChain>(*bento::common_allocator(), dx12_swapChain);
			}

			RenderTexture get_current_render_texture(SwapChain swapChain)
			{
				DX12SwapChain* dx12_swapChain = (DX12SwapChain*)swapChain;
				return (RenderTexture)(&dx12_swapChain->backBufferRenderTexture[dx12_swapChain->currentBackBuffer]);
			}

			void present(SwapChain swapChain, CommandQueue commandQueue)
			{
				// Convert to the internal structure
				DX12SwapChain* dx12_swapChain = (DX12SwapChain*)swapChain;

				// Present the frame buffer
				assert_msg(dx12_swapChain->swapChain->Present(0, 0) == S_OK, "Swap Chain Present failed.");

				// Wait on the fence
				DX12CommandQueue* dx12_commandQueue = (DX12CommandQueue*)commandQueue;
				command_queue::flush(commandQueue);

				// Update the current back buffer
				dx12_swapChain->currentBackBuffer = dx12_swapChain->swapChain->GetCurrentBackBufferIndex();
			}
		}

		// Command Buffer API
		namespace command_buffer
		{
			void change_resource_state(DX12CommandBuffer* commandBuffer, ID3D12Resource* resource, D3D12_RESOURCE_STATES& resourceState, D3D12_RESOURCE_STATES targetState)
			{
				if (targetState != resourceState)
				{
					// Define a barrier for the resource
					D3D12_RESOURCE_BARRIER barrier = {};
					barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
					barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
					barrier.Transition.pResource = resource;
					barrier.Transition.StateBefore = resourceState;
					barrier.Transition.StateAfter = targetState;
					barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
					commandBuffer->cmdList->ResourceBarrier(1, &barrier);

					// Keep track of the new state
					resourceState = targetState;
				}
			}

			CommandBuffer create_command_buffer(GraphicsDevice graphicsDevice)
			{
				bento::IAllocator* allocator = bento::common_allocator();
				assert(allocator != nullptr);

				// Grab the graphics device
				DX12GraphicsDevice* dx12_device = (DX12GraphicsDevice*)graphicsDevice;

				// Create the command buffer
				DX12CommandBuffer* dx12_commandBuffer = bento::make_new<DX12CommandBuffer>(*allocator);

				// Create the command allocator i
				assert_msg(dx12_device->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&dx12_commandBuffer->cmdAlloc)) == S_OK, "Failed to create command allocator");
				
				// Create the command list
				assert_msg(dx12_device->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT, dx12_commandBuffer->cmdAlloc, nullptr, IID_PPV_ARGS(&dx12_commandBuffer->cmdList)) == S_OK, "Failed to create command list.");
				assert_msg(dx12_commandBuffer->cmdList->Close() == S_OK, "Failed to close command list.");
				dx12_commandBuffer->deviceI = dx12_device;

				// Convert to the opaque structure
				return (CommandBuffer)dx12_commandBuffer;
			}

			void destroy_command_buffer(CommandBuffer command_buffer)
			{
				// Convert to the internal structure
				DX12CommandBuffer* dx12_commandBuffer = (DX12CommandBuffer*)command_buffer;

				// Release the command list
				dx12_commandBuffer->cmdList->Release();

				// Release the command allocator
				dx12_commandBuffer->cmdAlloc->Release();

				// Destroy the render environment
				bento::make_delete<DX12CommandBuffer>(*bento::common_allocator(), dx12_commandBuffer);
			}

			void reset(CommandBuffer commandBuffer)
			{
				DX12CommandBuffer* dx12_commandBuffer = (DX12CommandBuffer*)commandBuffer;
				dx12_commandBuffer->cmdAlloc->Reset();
				dx12_commandBuffer->cmdList->Reset(dx12_commandBuffer->cmdAlloc, nullptr);
			}

			void close(CommandBuffer commandBuffer)
			{
				DX12CommandBuffer* dx12_commandBuffer = (DX12CommandBuffer*)commandBuffer;
				dx12_commandBuffer->cmdList->Close();
			}

			void set_render_texture(CommandBuffer commandBuffer, RenderTexture renderTexture)
			{
				DX12CommandBuffer* dx12_commandBuffer = (DX12CommandBuffer*)commandBuffer;
				DX12RenderTexture* dx12_renderTexture = (DX12RenderTexture*)renderTexture;

				// Grab the render target view
				D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(dx12_renderTexture->descriptorHeap->GetCPUDescriptorHandleForHeapStart());
				rtvHandle.ptr += dx12_renderTexture->heapOffset;

				// Set the render target and the current one
				dx12_commandBuffer->cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
			}

			void clear_render_texture(CommandBuffer commandBuffer, RenderTexture renderTeture, const bento::Vector4& color)
			{
				// Grab the actual structures
				DX12CommandBuffer* dx12_commandBuffer = (DX12CommandBuffer*)commandBuffer;
				DX12RenderTexture* dx12_renderTexture = (DX12RenderTexture*)renderTeture;

				// Grab the render target view
				D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(dx12_renderTexture->descriptorHeap->GetCPUDescriptorHandleForHeapStart());
				rtvHandle.ptr += dx12_renderTexture->heapOffset;

				// Make sure the state is the right one
				change_resource_state(dx12_commandBuffer, dx12_renderTexture->resource, dx12_renderTexture->state, D3D12_RESOURCE_STATE_RENDER_TARGET);

				// Clear with the color
				dx12_commandBuffer->cmdList->ClearRenderTargetView(rtvHandle, &color.x, 0, nullptr);
			}

			void render_texture_present(CommandBuffer commandBuffer, RenderTexture renderTeture)
			{
				DX12CommandBuffer* dx12_commandBuffer = (DX12CommandBuffer*)commandBuffer;
				DX12RenderTexture* dx12_renderTexture = (DX12RenderTexture*)renderTeture;

				// Make sure the state is the right one
				change_resource_state(dx12_commandBuffer, dx12_renderTexture->resource, dx12_renderTexture->state, D3D12_RESOURCE_STATE_PRESENT);
			}

			void copy_graphics_buffer(CommandBuffer commandBuffer, GraphicsBuffer inputBuffer, GraphicsBuffer outputBuffer)
			{
				// Get the internal command buffer structure
				DX12CommandBuffer* dx12_commandBuffer = (DX12CommandBuffer*)commandBuffer;

				// Prepare the input buffer if needed
				DX12GraphicsBuffer* dx12_inputBuffer = (DX12GraphicsBuffer*)inputBuffer;
				change_resource_state(dx12_commandBuffer, dx12_inputBuffer->resource, dx12_inputBuffer->state, dx12_inputBuffer->type == GraphicsBufferType::Upload ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_SOURCE);

				// Prepare the output buffer if needed
				DX12GraphicsBuffer* dx12_outputBuffer = (DX12GraphicsBuffer*)outputBuffer;
				change_resource_state(dx12_commandBuffer, dx12_outputBuffer->resource, dx12_outputBuffer->state, D3D12_RESOURCE_STATE_COPY_DEST);

				// Copy the resource
				dx12_commandBuffer->cmdList->CopyResource(dx12_outputBuffer->resource, dx12_inputBuffer->resource);
			}

			void set_compute_graphics_buffer_uav(CommandBuffer commandBuffer, ComputeShader computeShader, uint32_t slot, GraphicsBuffer graphicsBuffer)
			{
				// Grab all the internal structures
				DX12CommandBuffer* dx12_commandBuffer = (DX12CommandBuffer*)commandBuffer;
				DX12GraphicsDevice* deviceI = dx12_commandBuffer->deviceI;
				DX12ComputeShader* dx12_cs = (DX12ComputeShader*)computeShader;
				DX12GraphicsBuffer* buffer = (DX12GraphicsBuffer*)graphicsBuffer;

				// Create the view in the compute's heap
				D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
				ZeroMemory(&uavDesc, sizeof(D3D12_UNORDERED_ACCESS_VIEW_DESC));
				uavDesc.Format = DXGI_FORMAT_UNKNOWN;
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
				D3D12_BUFFER_UAV bufferUAV;
				bufferUAV.FirstElement = 0;
				bufferUAV.NumElements = buffer->bufferSize / (uint64_t)buffer->elementSize;
				bufferUAV.StructureByteStride = buffer->elementSize;
				bufferUAV.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
				bufferUAV.CounterOffsetInBytes = 0;
				uavDesc.Buffer = bufferUAV;
				deviceI->device->CreateUnorderedAccessView(buffer->resource, nullptr, &uavDesc, dx12_cs->uavCPU);

				// Change the resource's state
				change_resource_state(dx12_commandBuffer, buffer->resource, buffer->state, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			}

			void set_compute_graphics_buffer_srv(CommandBuffer commandBuffer, ComputeShader computeShader, uint32_t slot, GraphicsBuffer graphicsBuffer)
			{
				// Grab all the internal structures
				DX12CommandBuffer* dx12_commandBuffer = (DX12CommandBuffer*)commandBuffer;
				DX12GraphicsDevice* deviceI = dx12_commandBuffer->deviceI;
				DX12ComputeShader* dx12_cs = (DX12ComputeShader*)computeShader;
				DX12GraphicsBuffer* buffer = (DX12GraphicsBuffer*)graphicsBuffer;

				// Create the view in the compute's heap
				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
				srvDesc.Format = DXGI_FORMAT_UNKNOWN;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				srvDesc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0, D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1, D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2, D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3);
				D3D12_BUFFER_SRV bufferSRV;
				bufferSRV.FirstElement = 0;
				bufferSRV.NumElements = buffer->bufferSize / (uint64_t)buffer->elementSize;
				bufferSRV.StructureByteStride = buffer->elementSize;
				bufferSRV.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
				srvDesc.Buffer = bufferSRV;
				deviceI->device->CreateShaderResourceView(buffer->resource, &srvDesc, dx12_cs->srvCPU);

				// Change the resource's state
				change_resource_state(dx12_commandBuffer, buffer->resource, buffer->state, D3D12_RESOURCE_STATE_COMMON);
			}

			void dispatch(CommandBuffer commandBuffer, ComputeShader computeShader, uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ)
			{
				DX12CommandBuffer* dx12_cmd = (DX12CommandBuffer*)commandBuffer;
				DX12ComputeShader* dx12_cs = (DX12ComputeShader*)computeShader;

				// Bind the root descriptor tables
				ID3D12DescriptorHeap* ppHeaps[] = { dx12_cs->descriptorHeap};
				dx12_cmd->cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
				dx12_cmd->cmdList->SetComputeRootSignature(dx12_cs->rootSignature);
				dx12_cmd->cmdList->SetComputeRootDescriptorTable(0, dx12_cs->srvGPU);
				dx12_cmd->cmdList->SetComputeRootDescriptorTable(1, dx12_cs->uavGPU);

				// Bind the shader and dispatch it
				dx12_cmd->cmdList->SetPipelineState(dx12_cs->pipelineStateObject);
				dx12_cmd->cmdList->Dispatch(sizeX, sizeY, sizeZ);
			}
		}

		namespace fence
		{
			Fence create_fence(GraphicsDevice graphicsDevice)
			{
				DX12GraphicsDevice* dx12_device = (DX12GraphicsDevice*)graphicsDevice;
				ID3D12Fence* dx12_fence;
				assert_msg(dx12_device->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&dx12_fence)) == S_OK, "Failed to create Fence");
				return (Fence)dx12_fence;
			}

			void destroy_fence(Fence fence)
			{
				ID3D12Fence* fenceDX = (ID3D12Fence*)fence;
				fenceDX->Release();
			}

			FenceEvent create_fence_event()
			{
				HANDLE fenceEvent;

				fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
				assert_msg(fenceEvent != nullptr, "Failed to create fence event.");

				return (FenceEvent)fenceEvent;
			}

			void destroy_fence_event(FenceEvent fenceEvent)
			{
				HANDLE fenceEventDX = (HANDLE)fenceEvent;
				CloseHandle(fenceEventDX);
			}

			void wait_for_fence_value(Fence fence, uint64_t fenceValue, FenceEvent fenceEvent, uint64_t maxTime)
			{
				ID3D12Fence* fenceDX = (ID3D12Fence*)fence;
				HANDLE fenceEventDX = (HANDLE)fenceEvent;
				if (fenceDX->GetCompletedValue() < fenceValue)
				{
					assert_msg(fenceDX->SetEventOnCompletion(fenceValue, fenceEventDX) == S_OK, "Failed to wait on fence.");
					WaitForSingleObject(fenceEventDX, (DWORD)maxTime);
				}
			}
		}

		DXGI_FORMAT graphics_format_to_dxgi_format(GraphicsFormat graphicsFormat)
		{
			switch (graphicsFormat)
			{
				// R8G8B8A8 Formats
				case GraphicsFormat::R8G8B8A8_SNorm:
					return DXGI_FORMAT_R8G8B8A8_SNORM;
				case GraphicsFormat::R8G8B8A8_UNorm:
					return DXGI_FORMAT_R8G8B8A8_UNORM;
				case GraphicsFormat::R8G8B8A8_UInt:
					return DXGI_FORMAT_R8G8B8A8_UINT;
				case GraphicsFormat::R8G8B8A8_SInt:
					return DXGI_FORMAT_R8G8B8A8_SINT;

				// R16G16B16A16 Formats
				case GraphicsFormat::R16G16B16A16_SFloat:
					return DXGI_FORMAT_R16G16B16A16_FLOAT;
				case GraphicsFormat::R16G16B16A16_UInt:
					return DXGI_FORMAT_R16G16B16A16_UINT;
				case GraphicsFormat::R16G16B16A16_SInt:
					return DXGI_FORMAT_R16G16B16A16_SINT;

				// Depth/Stencil formats
				case GraphicsFormat::Depth32:
					return DXGI_FORMAT_D32_FLOAT;
				case GraphicsFormat::Depth24Stencil8:
					return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
			}

			// Should never be here
			assert_fail_msg("Unknown DX12 Format");
			return DXGI_FORMAT_R8G8B8A8_SNORM;
		}

		D3D12_RESOURCE_DIMENSION texture_dimension_to_dx12_resource_dimension(TextureDimension textureDimension)
		{
			switch (textureDimension)
			{
			case TextureDimension::Tex1D:
			case TextureDimension::Tex1DArray:
				return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
			case TextureDimension::Tex2D:
			case TextureDimension::TexCube:
				return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			case TextureDimension::Tex3D:
			case TextureDimension::TexCubeArray:
			case TextureDimension::Tex2DArray:
				return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
			default:
				return D3D12_RESOURCE_DIMENSION_UNKNOWN;
			}
		}

		namespace graphics_resources
		{
			RenderTexture create_render_texture(GraphicsDevice graphicsDevice, RenderTextureDescriptor rtDesc)
			{
				DX12GraphicsDevice* deviceI = (DX12GraphicsDevice*)graphicsDevice;
				ID3D12Device2* device = deviceI->device;
				assert(deviceI != nullptr);

				// Define the heap
				D3D12_HEAP_PROPERTIES heapProperties = {};
				heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
				heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
				heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

				// Not sure about this
				heapProperties.VisibleNodeMask = 0xff;

				// Define the clear value
				D3D12_CLEAR_VALUE clearValue;
				clearValue.Format = graphics_format_to_dxgi_format(rtDesc.format);
				memcpy(clearValue.Color, &rtDesc.clearColor.x, 4 * sizeof(float));

				D3D12_RESOURCE_DESC resourceDescriptor = {};
				resourceDescriptor.Dimension = texture_dimension_to_dx12_resource_dimension(rtDesc.dimension);
				resourceDescriptor.Width = rtDesc.width;
				resourceDescriptor.Height = rtDesc.height;
				resourceDescriptor.DepthOrArraySize = rtDesc.depth;
				resourceDescriptor.MipLevels = rtDesc.hasMips ? 2 : 0;
				resourceDescriptor.Format = graphics_format_to_dxgi_format(rtDesc.format);
				resourceDescriptor.SampleDesc = DXGI_SAMPLE_DESC{ 1, 0 };
				resourceDescriptor.Alignment = graphics_format_alignement(rtDesc.format);

				// This is a choice for now
				resourceDescriptor.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

				// Raise all the relevant flags
				resourceDescriptor.Flags = D3D12_RESOURCE_FLAG_NONE;
				resourceDescriptor.Flags |= rtDesc.isUAV ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
				resourceDescriptor.Flags |= is_depth_format(rtDesc.format) ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
				
				// Resource states
				D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
				state |= rtDesc.isUAV ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_COMMON;
				state |= is_depth_format(rtDesc.format) ? D3D12_RESOURCE_STATE_DEPTH_WRITE | D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_RENDER_TARGET;

				// Create the render target
				ID3D12Resource* resource;
				assert_msg(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES, &resourceDescriptor, state, &clearValue, IID_PPV_ARGS(&resource)) == S_OK, "Failed to create render target.");
				
				// Create the descriptor heap
				ID3D12DescriptorHeap* descriptorHeap = create_descriptor_heap(device, 1, is_depth_format(rtDesc.format) ? D3D12_DESCRIPTOR_HEAP_TYPE_DSV : D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

				// Create a render target view for it
				D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());
				if (rtDesc.isUAV)
					device->CreateUnorderedAccessView(resource, nullptr, nullptr, rtvHandle);
				else if (is_depth_format(rtDesc.format))
					device->CreateDepthStencilView(resource, nullptr, rtvHandle);
				else
					device->CreateRenderTargetView(resource, nullptr, rtvHandle);

				bento::IAllocator* allocator = bento::common_allocator();
				assert(allocator != nullptr);

				// Create the render texture internal structure
				DX12RenderTexture* dx12_renderTexture = bento::make_new<DX12RenderTexture>(*allocator);
				dx12_renderTexture->resource = resource;
				dx12_renderTexture->descriptorHeap = descriptorHeap;
				dx12_renderTexture->heapOffset = 0;

				// Return the render target
				return (RenderTexture)dx12_renderTexture;
			}

			void destroy_render_texture(RenderTexture renderTexture)
			{
				DX12RenderTexture* dx12_renderTexture = (DX12RenderTexture*)renderTexture;
				if (dx12_renderTexture->rtOwned)
					dx12_renderTexture->descriptorHeap->Release();
				dx12_renderTexture->resource->Release();
			}

			GraphicsBuffer create_graphics_buffer(GraphicsDevice graphicsDevice, uint64_t bufferSize, uint64_t elementSize, GraphicsBufferType bufferType)
			{
				DX12GraphicsDevice* dx12_device = (DX12GraphicsDevice*)graphicsDevice;
				assert(dx12_device != nullptr);

				// Define the heap
				D3D12_HEAP_PROPERTIES heapProperties = {};
				heapProperties.Type = bufferType == GraphicsBufferType::Default ? D3D12_HEAP_TYPE_DEFAULT : (bufferType == GraphicsBufferType::Upload ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_READBACK);
				heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
				heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
				heapProperties.CreationNodeMask = 0;
				heapProperties.VisibleNodeMask = 0;

				// Define the resource descriptor
				D3D12_RESOURCE_DESC resourceDescriptor = { D3D12_RESOURCE_DIMENSION_BUFFER, 0, bufferSize, 1, 1, 1,
				DXGI_FORMAT_UNKNOWN, 1, 0, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, bufferType == GraphicsBufferType::Default ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE };

				// Resource states
				D3D12_RESOURCE_STATES state;
				if (bufferType == GraphicsBufferType::Upload)
					state = D3D12_RESOURCE_STATE_GENERIC_READ;
				else if (bufferType == GraphicsBufferType::Readback)
					state = D3D12_RESOURCE_STATE_COPY_DEST;
				else
					state = D3D12_RESOURCE_STATE_COMMON;

				// Create the resource
				ID3D12Resource* buffer;
				assert_msg(dx12_device->device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDescriptor, state, nullptr, IID_PPV_ARGS(&buffer)) == S_OK, "Failed to create the graphics buffer.");

				// Create the buffer internal structure
				DX12GraphicsBuffer* dx12_graphicsBuffer = bento::make_new<DX12GraphicsBuffer>(*bento::common_allocator());
				dx12_graphicsBuffer->resource = buffer;
				dx12_graphicsBuffer->state = state;
				dx12_graphicsBuffer->type = bufferType;
				dx12_graphicsBuffer->bufferSize = bufferSize;
				dx12_graphicsBuffer->elementSize = elementSize;
				
				// Return the opaque structure
				return (GraphicsBuffer)dx12_graphicsBuffer;
			}

			void destroy_graphics_buffer(GraphicsBuffer graphicsBuffer)
			{
				DX12GraphicsBuffer* dx12_buffer = (DX12GraphicsBuffer*)graphicsBuffer;
				dx12_buffer->resource->Release();
				bento::make_delete<DX12GraphicsBuffer>(*bento::common_allocator(), dx12_buffer);
			}

			void set_data(GraphicsBuffer graphicsBuffer, char* buffer, uint64_t bufferSize)
			{
				// Convert to the internal structure 
				DX12GraphicsBuffer* dx12_buffer = (DX12GraphicsBuffer*)graphicsBuffer;

				// If this is not an upload buffer, we can't do anything here
				if (dx12_buffer->type != GraphicsBufferType::Upload)
					return;

				// Copy to the CPU buffer
				uint8_t* cpuBuffer = nullptr;
				D3D12_RANGE readRange = { 0, 0 };
				dx12_buffer->resource->Map(0, &readRange, reinterpret_cast<void **>(&cpuBuffer));
				memcpy(cpuBuffer, buffer, bufferSize);
				dx12_buffer->resource->Unmap(0, nullptr);
			}

			char* allocate_cpu_buffer(GraphicsBuffer graphicsBuffer)
			{
				// Get the actual resource
				DX12GraphicsBuffer* dx12_buffer = (DX12GraphicsBuffer*)graphicsBuffer;

				// If this is not a readback buffer, just stop
				if (dx12_buffer->type != GraphicsBufferType::Readback)
					return nullptr;

				// Map it
				char* data = nullptr;
				D3D12_RANGE range = { 0, dx12_buffer->bufferSize};
				dx12_buffer->resource->Map(0, &range, (void**)&data);
				return data;
			}

			void release_cpu_buffer(GraphicsBuffer graphicsBuffer)
			{
				DX12GraphicsBuffer* dx12_buffer = (DX12GraphicsBuffer*)graphicsBuffer;
				dx12_buffer->resource->Unmap(0, nullptr);
			}
		}

		namespace compute_shader
		{
			ComputeShader create_compute_shader(GraphicsDevice graphicsDevice, const ComputeShaderDescriptor& csd)
			{
				// Convert the strings to wide
				const std::wstring& filename = convert_to_wide(csd.filename.c_str(), csd.filename.size());
				const std::wstring& kernelName = convert_to_wide(csd.kernelname.c_str(), csd.kernelname.size());

				// Create the library and the compiler
				IDxcLibrary* library;
				DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));

				// Load the file into a blob
				uint32_t code_page = CP_UTF8;
				IDxcBlobEncoding* source_blob;
				assert_msg(library->CreateBlobFromFile(filename.c_str(), &code_page, &source_blob) == S_OK, "Failed to load the shader code.");

				// Release the library
				library->Release();

				// Compilation arguments
				LPCWSTR arguments[] = { L"-O3"};

				// Create the compiler
				IDxcCompiler* compiler;
				DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));

				// Compile the shader
				IDxcOperationResult* result;
				HRESULT hr = compiler->Compile(source_blob, filename.c_str(), kernelName.c_str(), L"cs_6_4", arguments, _countof(arguments), nullptr, 0, nullptr, &result);
				if (SUCCEEDED(hr))
					result->GetStatus(&hr);
				bool compile_succeed = SUCCEEDED(hr);

				// If the compilation failed, print the error
				IDxcBlobEncoding* error_blob;
				if (SUCCEEDED(result->GetErrorBuffer(&error_blob)) && error_blob)
				{
					// Log the compilation message
					bento::default_logger()->log(bento::LogLevel::info, "Compilation", error_blob->GetBufferSize() != 0 ? (const char*)error_blob->GetBufferPointer() : "Successfully compiled kernel.");

					// Release the error blob
					error_blob->Release();
				}

				// If succeeded, grab the right pointer
				IDxcBlob* shader_blob = 0;
				if (compile_succeed)
					result->GetResult(&shader_blob);

				// Release all the intermediate resources
				result->Release();
				source_blob->Release();
				compiler->Release();
	
				// Grab the graphics device
				DX12GraphicsDevice* deviceI = (DX12GraphicsDevice*)graphicsDevice;
				assert(deviceI != nullptr);
				ID3D12Device2* device = deviceI->device;

				// Create the root signature for the shader
				D3D12_ROOT_PARAMETER rootParameters[3];
				D3D12_DESCRIPTOR_RANGE descRange[3];

				// Tracking the current count/index
				uint8_t cdIndex = 0;

				// Process the SRVS
				if (csd.srvCount > 0)
				{
					descRange[cdIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
					descRange[cdIndex].NumDescriptors = csd.srvCount;
					descRange[cdIndex].BaseShaderRegister = 0; // t0..tN
					descRange[cdIndex].RegisterSpace = 0;
					descRange[cdIndex].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
					rootParameters[cdIndex].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
					rootParameters[cdIndex].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
					rootParameters[cdIndex].DescriptorTable.NumDescriptorRanges = 1;
					rootParameters[cdIndex].DescriptorTable.pDescriptorRanges = &descRange[cdIndex];
					cdIndex++;
				}

				// Process the UAVs
				if (csd.uavCount > 0)
				{
					descRange[cdIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
					descRange[cdIndex].NumDescriptors = csd.uavCount;
					descRange[cdIndex].BaseShaderRegister = 0; // u0..uN
					descRange[cdIndex].RegisterSpace = 0;
					descRange[cdIndex].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
					rootParameters[cdIndex].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
					rootParameters[cdIndex].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
					rootParameters[cdIndex].DescriptorTable.NumDescriptorRanges = 1;
					rootParameters[cdIndex].DescriptorTable.pDescriptorRanges = &descRange[cdIndex];
					cdIndex++;
				}

				// Process the CBVs
				if (csd.cbvCount > 0)
				{
					descRange[cdIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
					descRange[cdIndex].NumDescriptors = csd.cbvCount;
					descRange[cdIndex].BaseShaderRegister = 0; // b0..bN
					descRange[cdIndex].RegisterSpace = 0;
					descRange[cdIndex].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
					rootParameters[cdIndex].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
					rootParameters[cdIndex].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
					rootParameters[cdIndex].DescriptorTable.NumDescriptorRanges = 1;
					rootParameters[cdIndex].DescriptorTable.pDescriptorRanges = &descRange[cdIndex];
					cdIndex++;
				}

				D3D12_ROOT_SIGNATURE_DESC desc = {};
				desc.NumParameters = cdIndex;
				desc.pParameters = rootParameters;
				desc.NumStaticSamplers = 0;
				desc.pStaticSamplers = nullptr;

				// Create the signature blob
				ID3DBlob* signatureBlob;
				assert_msg(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, nullptr) == S_OK, "Failed to create root singnature blob.");

				// Create the root signature
				ID3D12RootSignature* rootSignature;
				assert_msg(device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)) == S_OK, "Failed to create root signature.");

				// Release the resources
				signatureBlob->Release();

				// Create the descriptor heap for this compute shader
				ID3D12DescriptorHeap* descHeap = create_descriptor_heap(device, csd.srvCount + csd.uavCount + csd.cbvCount, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				
				// Create the pipeline state object for the shader
				D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.pRootSignature = rootSignature;
				pso_desc.CS.BytecodeLength = shader_blob->GetBufferSize();
				pso_desc.CS.pShaderBytecode = shader_blob->GetBufferPointer();
				ID3D12PipelineState* pso;
				assert_msg(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pso)) == S_OK, "Failed to create pipeline state object.");

				// Create the compute shader structure
				DX12ComputeShader* cS = bento::make_new<DX12ComputeShader>(*bento::common_allocator());
				cS->shaderBlob = shader_blob;
				cS->rootSignature = rootSignature;
				cS->pipelineStateObject = pso;
				cS->descriptorHeap = descHeap;

				// Number of resources per type
				cS->srvCount = csd.srvCount;
				cS->uavCount = csd.uavCount;
				cS->cbvCount = csd.cbvCount;

				// Pre-evaluate the CPU Heap handles
				uint32_t descSize = deviceI->descriptorSize[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
				cS->srvCPU = descHeap->GetCPUDescriptorHandleForHeapStart();
				cS->uavCPU = cS->srvCPU;
				cS->uavCPU.ptr += csd.srvCount * descSize;
				cS->cbvCPU = cS->uavCPU;
				cS->cbvCPU.ptr += csd.uavCount * descSize;

				// Pre-evaluate the GPU Heap handles
				cS->srvGPU = descHeap->GetGPUDescriptorHandleForHeapStart();
				cS->uavGPU = cS->srvGPU;
				cS->uavGPU.ptr += csd.srvCount * descSize;
				cS->cbvGPU = cS->uavGPU;
				cS->cbvGPU.ptr += csd.uavCount * descSize;

				// Convert to the opaque structure
				return (ComputeShader)cS;
			}

			void destroy_compute_shader(ComputeShader computeShader)
			{
				DX12ComputeShader* dx12_computeShader = (DX12ComputeShader*)computeShader;
				dx12_computeShader->descriptorHeap->Release();
				dx12_computeShader->rootSignature->Release();
				dx12_computeShader->shaderBlob->Release();
				bento::make_delete<DX12ComputeShader>(*bento::common_allocator(), dx12_computeShader);
			}
		}
	}
}