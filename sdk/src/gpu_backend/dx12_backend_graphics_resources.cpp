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
				ID3D12DescriptorHeap* descHeap = (ID3D12DescriptorHeap*)descriptor_heap::create_descriptor_heap(graphicsDevice, 1, is_depth_format(rtDesc.format) ? D3D12_DESCRIPTOR_HEAP_TYPE_DSV : D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

				// Create a render target view for it
				D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(descHeap->GetCPUDescriptorHandleForHeapStart());
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
				dx12_renderTexture->descriptorHeap = descHeap;
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

			GraphicsBuffer create_graphics_buffer(GraphicsDevice graphicsDevice, uint64_t bufferSize, uint32_t elementSize, GraphicsBufferType bufferType)
			{
				DX12GraphicsDevice* deviceI = (DX12GraphicsDevice*)graphicsDevice;

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
				assert_msg(deviceI->device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDescriptor, state, nullptr, IID_PPV_ARGS(&buffer)) == S_OK, "Failed to create the graphics buffer.");

				// Create the buffer internal structure
				DX12GraphicsBuffer* dx12_graphicsBuffer = bento::make_new<DX12GraphicsBuffer>(*bento::common_allocator());
				dx12_graphicsBuffer->resource = buffer;
				dx12_graphicsBuffer->state = state;
				dx12_graphicsBuffer->type = bufferType;
				dx12_graphicsBuffer->bufferSize = bufferSize;
				dx12_graphicsBuffer->elementSize = (uint32_t)elementSize;
				
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

			ConstantBuffer create_constant_buffer(GraphicsDevice graphicsDevice, uint32_t bufferSize, uint64_t elementSize)
			{
				// The size needs to be aligned on 256
				uint32_t alignedSize = (bufferSize + (DX12_CONSTANT_BUFFER_ALIGNEMENT_SIZE - 1)) / DX12_CONSTANT_BUFFER_ALIGNEMENT_SIZE;
				DX12GraphicsBuffer* buffer = (DX12GraphicsBuffer*)create_graphics_buffer(graphicsDevice, alignedSize * DX12_CONSTANT_BUFFER_ALIGNEMENT_SIZE, elementSize, GraphicsBufferType::Upload);
				return (ConstantBuffer)buffer;
			}

			void destroy_constant_buffer(ConstantBuffer constantBuffer)
			{
				DX12GraphicsBuffer* buffer = (DX12GraphicsBuffer*)constantBuffer;
				destroy_graphics_buffer((GraphicsBuffer)buffer);
			}

			void upload_constant_buffer(ConstantBuffer constantBuffer, const char* bufferData, uint32_t bufferSize)
			{
				DX12GraphicsBuffer* buffer = (DX12GraphicsBuffer*)constantBuffer;
				D3D12_RANGE readRange = { 0, 0 };
				uint8_t* cbvDataBegin;
				buffer->resource->Map(0, &readRange, reinterpret_cast<void**>(&cbvDataBegin));
				memcpy(cbvDataBegin, bufferData, bufferSize);
				buffer->resource->Unmap(0, nullptr);
			}
		}
	}
}
