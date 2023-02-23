// Bento includes
#include <bento_base/security.h>
#include <bento_memory/common.h>
#include <bento_base/log.h>

// Internal includes
#include "gpu_backend/dx12_backend.h"
#include "gpu_backend/dx12_containers.h"
#include "gpu_backend/dx12_utilities.h"

namespace graphics_sandbox
{
	namespace d3d12
	{
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

		namespace descriptor_heap
		{
			DescriptorHeap create_descriptor_heap(GraphicsDevice graphicsDevice, uint32_t numDescriptors, uint32_t opaqueType)
			{
				// Get the actual type
				D3D12_DESCRIPTOR_HEAP_TYPE type = (D3D12_DESCRIPTOR_HEAP_TYPE)opaqueType;
				DX12GraphicsDevice* deviceI = (DX12GraphicsDevice*)graphicsDevice;
				ID3D12Device2* device = deviceI->device;

				D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
				rtvHeapDesc.NumDescriptors = numDescriptors;
				rtvHeapDesc.Type = type;
				if (type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV || type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
					rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
				else
					rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

				ID3D12DescriptorHeap* descriptorHeap;
				assert_msg(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&descriptorHeap)) == S_OK, "Failed to create descriptor heap.");
				return (DescriptorHeap)descriptorHeap;
			}

			void destroy_descriptor_heap(DescriptorHeap oDescriptorHeap)
			{
				ID3D12DescriptorHeap* descriptorHeap = (ID3D12DescriptorHeap*)oDescriptorHeap;
				descriptorHeap->Release();
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
				swapChainI->descriptorHeap = (ID3D12DescriptorHeap*)descriptor_heap::create_descriptor_heap(graphicsDevice, DX12_NUM_BACK_BUFFERS, (uint32_t)D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

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
	}
}