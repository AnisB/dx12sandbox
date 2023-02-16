// Bento includes
#include <bento_base/security.h>
#include <bento_memory/common.h>

// DX12 and windows includes
#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <Windows.h>


// Generic includes
#include <algorithm>

// Internal includes
#include "gpu_backend/dx12_backend.h"

// Required for ComPtr
#define DX12_NUM_FRAMES 3

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, message, wParam, lParam); // add this
	}
	return 0;
}
 
namespace graphics_sandbox
{
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

		struct DX12RenderEnvironment
		{
			// Window data structure
			HWND window;

			// Graphics device
			ID3D12Device2* device;

			// Global command queue
			ID3D12CommandQueue* commandQueue;

			// Swap chain
			IDXGISwapChain4* swapChain;
			uint32_t currentBackBuffer;

			// Description heap required for the swap chain
			ID3D12DescriptorHeap* descriptorHeap;
			uint32_t rtvDescriptorSize;

			// Back Buffers
			ID3D12Resource* backBufferRenderTargetViews[DX12_NUM_FRAMES];
		};

		// Function to enable the debug layer
		void EnableDebugLayer()
		{
			ID3D12Debug* debugInterface;
			assert_msg(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)) == S_OK, "Debug layer failed to initialize");
			debugInterface->EnableDebugLayer();
			debugInterface->Release();
		}

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

		// On DX12 to create a graphics device, we need to fetch the adapter of the right device.
		// This is what this function does
		IDXGIAdapter4* GetAdapter()
		{
			// Create the DXGI factory
			IDXGIFactory4* dxgiFactory;
			UINT createFactoryFlags = 0;
			assert_msg(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)) == S_OK, "DXGI Factory 2 failed.");

			// Loop through all the available dapters
			IDXGIAdapter1* dxgiAdapter1;
			IDXGIAdapter4* dxgiAdapter4;
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

		// Function that actually create the DX12 device
		ID3D12Device2* CreateDevice()
		{
			IDXGIAdapter4* adapter = GetAdapter();

			// Create the graphics device and ensure it's been succesfully created
			ID3D12Device2* d3d12Device2;
			assert_msg(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2)) == S_OK, "D3D12 Device creation failed.");

			// Do not forget to release the resources
			adapter->Release();

			// Return the device
			return d3d12Device2;
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


		ID3D12DescriptorHeap* CreateDescriptorHeap(ID3D12Device2* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors)
		{
			ID3D12DescriptorHeap* descriptorHeap;

			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.NumDescriptors = numDescriptors;
			desc.Type = type;

			assert_msg(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)) == S_OK, "Failed to create descriptor heap.");

			return descriptorHeap;
		}

		void CreateRenderTargetViews(DX12RenderEnvironment* dx12_re)
		{
			auto rtvDescriptorSize = dx12_re->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(dx12_re->descriptorHeap->GetCPUDescriptorHandleForHeapStart());

			// Loop through the frames of the swap chain
			for (int i = 0; i < DX12_NUM_FRAMES; ++i)
			{
				// Grab the buffer
				ID3D12Resource* backBuffer;
				assert_msg(dx12_re->swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)) == S_OK, "Failed to get swap chain buffer.");

				// Create a render target view for it
				dx12_re->device->CreateRenderTargetView(backBuffer, nullptr, rtvHandle);

				// Keep track of the buffer
				dx12_re->backBufferRenderTargetViews[i] = backBuffer;

				// Move on to the next rtv
				rtvHandle.Offset(rtvDescriptorSize);
			}
		}

		/*
		HANDLE CreateEventHandle()
		{
			HANDLE fenceEvent;

			fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
			assert(fenceEvent && "Failed to create fence event.");

			return fenceEvent;
		}

			uint64_t Signal(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence,
				uint64_t& fenceValue)
		{
			uint64_t fenceValueForSignal = ++fenceValue;
			ThrowIfFailed(commandQueue->Signal(fence.Get(), fenceValueForSignal));

			return fenceValueForSignal;
		}

			void WaitForFenceValue(ComPtr<ID3D12Fence> fence, uint64_t fenceValue, HANDLE fenceEvent,
				std::chrono::milliseconds duration = std::chrono::milliseconds::max())
			{
				if (fence->GetCompletedValue() < fenceValue)
				{
					ThrowIfFailed(fence->SetEventOnCompletion(fenceValue, fenceEvent));
					::WaitForSingleObject(fenceEvent, static_cast<DWORD>(duration.count()));
				}
			}


			void Flush(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence,
				uint64_t& fenceValue, HANDLE fenceEvent)
			{
				uint64_t fenceValueForSignal = Signal(commandQueue, fence, fenceValue);
				WaitForFenceValue(fence, fenceValueForSignal, fenceEvent);
			}

			*/

		namespace render_system
		{
			bool init_render_system()
			{
				return true;
			}

			void shutdown_render_system()
			{
			}

			RenderEnvironment create_render_environment(const TGraphicSettings& graphic_settings)
			{
				bento::IAllocator* allocator = bento::common_allocator();
				assert(allocator != nullptr);

				// Create the render environment internal structure
				DX12RenderEnvironment* dx12_re = bento::make_new<DX12RenderEnvironment>(*allocator);

				// Convert the name from normal to wide
				size_t stringSize = graphic_settings.window_name.size();
				std::wstring wc(stringSize, L'#');
				mbstowcs(&wc[0], graphic_settings.window_name.c_str(), stringSize);

				HINSTANCE hInst = (HINSTANCE)graphic_settings.data[0];

				// Register the window
				RegisterWindowClass(hInst, wc.c_str());

				// Create the window
				dx12_re->window = CreateWindowInternal(wc.c_str(), hInst, wc.c_str(), graphic_settings.width, graphic_settings.height);

				// Create the graphics device
				dx12_re->device = CreateDevice();

				// Create the command queue
				dx12_re->commandQueue = CreateCommandQueue(dx12_re->device, D3D12_COMMAND_LIST_TYPE_DIRECT);

				// Create the swap chain
				dx12_re->swapChain = CreateSwapChain(dx12_re->window, dx12_re->commandQueue, graphic_settings.width, graphic_settings.height, DX12_NUM_FRAMES);

				// Grab the current back buffer
				dx12_re->currentBackBuffer = dx12_re->swapChain->GetCurrentBackBufferIndex();

				// Create the descriptor heap for the swap chain
				dx12_re->descriptorHeap = CreateDescriptorHeap(dx12_re->device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, DX12_NUM_FRAMES);
				dx12_re->rtvDescriptorSize = dx12_re->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

				// Create the render target views for the swap chain
				CreateRenderTargetViews(dx12_re);

				// Return the opaque structure
				return (RenderEnvironment)dx12_re;
			}

			void destroy_render_environment(RenderEnvironment render_environment)
			{
				// Convert to the internal structure
				DX12RenderEnvironment* dx12_re = (DX12RenderEnvironment*)render_environment;

				// Release the render target views for the swap chain buffers
				for (uint32_t i = 0; i < DX12_NUM_FRAMES; ++i)
				{
					dx12_re->backBufferRenderTargetViews[i]->Release();
				}

				// Release the descriptor heap
				dx12_re->descriptorHeap->Release();

				// Release the swap chain
				dx12_re->swapChain->Release();
				dx12_re->swapChain = nullptr;

				// Release the command queue
				dx12_re->commandQueue->Release();
				dx12_re->commandQueue = nullptr;

				// Release the device
				dx12_re->device->Release();
				dx12_re->device = nullptr;

				// Destroy the window
				DestroyWindow(dx12_re->window);

				// Destroy the render environment
				bento::make_delete<DX12RenderEnvironment>(*bento::common_allocator(), dx12_re);
			}

			RenderWindow render_window(RenderEnvironment render_environment)
			{
				// Convert to the internal structure
				DX12RenderEnvironment* dx12_re = (DX12RenderEnvironment*)render_environment;
				return (RenderWindow)dx12_re->window;
			}

			void flush_command_queue(RenderEnvironment render_environment)
			{
				DX12RenderEnvironment* dx12_re = (DX12RenderEnvironment*)render_environment;
				dx12_re->commandQueue

			}
		}

		namespace window
		{
			void show(RenderWindow window)
			{
				HWND dx12_window = (HWND)window;
				ShowWindow(dx12_window, SW_SHOWDEFAULT);
			}

			void hide(RenderWindow window)
			{
				HWND dx12_window = (HWND)window;
				ShowWindow(dx12_window, SW_HIDE);
			}
		}

		namespace command_buffer
		{
			struct DX12CommandBuffer
			{
				ID3D12CommandAllocator* commandAllocator[DX12_NUM_FRAMES];
				ID3D12GraphicsCommandList* commandList;
			};

			CommandBuffer create_command_buffer(RenderEnvironment render_environment)
			{
				bento::IAllocator* allocator = bento::common_allocator();
				assert(allocator != nullptr);

				// Convert to the internal structure
				DX12RenderEnvironment* dx12_re = (DX12RenderEnvironment*)render_environment;

				// Create the command buffer
				DX12CommandBuffer* dx12_commandBuffer = bento::make_new<DX12CommandBuffer>(*allocator);

				// Create the allocators we need (one per frame)
				for (int i = 0; i < DX12_NUM_FRAMES; ++i)
				{
					// Create the command allocator i
					assert_msg(dx12_re->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&dx12_commandBuffer->commandAllocator[i])) == S_OK, "Failed to create command allocator");
				}
				
				// Create the command list
				ID3D12CommandAllocator* currentCommandAlloc = dx12_commandBuffer->commandAllocator[dx12_re->currentBackBuffer];
				assert_msg(dx12_re->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT, currentCommandAlloc, nullptr, IID_PPV_ARGS(&dx12_commandBuffer->commandList)) == S_OK, "Failed to create command list.");
				assert_msg(dx12_commandBuffer->commandList->Close() == S_OK, "Failed to close command list.");

				// Convert to the opaque structure
				return (CommandBuffer)dx12_commandBuffer;
			}

			void destroy_command_buffer(CommandBuffer command_buffer)
			{
				// Convert to the internal structure
				DX12CommandBuffer* dx12_commandBuffer = (DX12CommandBuffer*)command_buffer;

				// Release the command list
				dx12_commandBuffer->commandList->Release();

				// Release the command allocator
				for (int i = 0; i < DX12_NUM_FRAMES; ++i)
				{
					dx12_commandBuffer->commandAllocator[i]->Release();
				}

				// Destroy the render environment
				bento::make_delete<DX12CommandBuffer>(*bento::common_allocator(), dx12_commandBuffer);
			}
		}

		namespace graphics_fence
		{
			Fence create_fence(RenderEnvironment render_environment)
			{
				// Convert to the internal structure
				DX12RenderEnvironment* dx12_re = (DX12RenderEnvironment*)render_environment;
				ID3D12Fence* dx12_fence;
				assert_msg(dx12_re->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&dx12_fence)) == S_OK, "Failed to create Fence");
				return (Fence)dx12_fence;
			}

			void destroy_fence(Fence fence)
			{
				DX12RenderEnvironment* dx12_re = (DX12RenderEnvironment*)fence;
				ID3D12Fence* dx12_ence = (ID3D12Fence*)fence;
				dx12_ence->Release();
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
				HANDLE fenceEventHandle = (HANDLE)fenceEvent;
				CloseHandle(fenceEventHandle);
			}
		}
	}
}