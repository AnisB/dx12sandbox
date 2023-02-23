// Bento includes
#include <bento_base/security.h>
#include <bento_memory/common.h>
#include <bento_base/log.h>

// Internal includes
#include "gpu_backend/dx12_backend.h"
#include "gpu_backend/dx12_containers.h"

namespace graphics_sandbox
{
	namespace d3d12
	{
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

			GraphicsDevice create_graphics_device(bool enableDebug)
			{
				// Grab the allocator
				bento::IAllocator* allocator = bento::common_allocator();
				assert(allocator != nullptr);

				// Debug Interface
				ID3D12Debug* debugInterface = nullptr;
				if (enableDebug)
				{
					assert_msg(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)) == S_OK, "Debug layer failed to initialize");
					debugInterface->EnableDebugLayer();
				}

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
				if (dx12_device->debugLayer != nullptr)
					dx12_device->debugLayer->Release();
			}
		}
	}
}
