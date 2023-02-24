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
				bufferUAV.NumElements = (uint32_t)buffer->bufferSize / (uint32_t)buffer->elementSize;
				bufferUAV.StructureByteStride = buffer->elementSize;
				bufferUAV.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
				bufferUAV.CounterOffsetInBytes = 0;
				uavDesc.Buffer = bufferUAV;

				// Compute the slot on the heap
				D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(dx12_cs->uavCPU);
				rtvHandle.ptr += (uint64_t)deviceI->descriptorSize[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] * slot;

				// Create the UAV
				deviceI->device->CreateUnorderedAccessView(buffer->resource, nullptr, &uavDesc, rtvHandle);

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
				bufferSRV.NumElements = (uint32_t)buffer->bufferSize / (uint32_t)buffer->elementSize;
				bufferSRV.StructureByteStride = buffer->elementSize;
				bufferSRV.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
				srvDesc.Buffer = bufferSRV;

				// Compute the slot on the heap
				D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(dx12_cs->srvCPU);
				rtvHandle.ptr += (uint64_t)deviceI->descriptorSize[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] * slot;

				// Create the SRV
				deviceI->device->CreateShaderResourceView(buffer->resource, &srvDesc, rtvHandle);

				// Change the resource's state
				change_resource_state(dx12_commandBuffer, buffer->resource, buffer->state, D3D12_RESOURCE_STATE_COMMON);
			}

			void set_compute_graphics_buffer_cbv(CommandBuffer commandBuffer, ComputeShader computeShader, uint32_t slot, ConstantBuffer constantBuffer)
			{
				// Grab all the internal structures
				DX12CommandBuffer* dx12_commandBuffer = (DX12CommandBuffer*)commandBuffer;
				DX12GraphicsDevice* deviceI = dx12_commandBuffer->deviceI;
				DX12ComputeShader* dx12_cs = (DX12ComputeShader*)computeShader;
				DX12GraphicsBuffer* buffer = (DX12GraphicsBuffer*)constantBuffer;

				// Create the view in the compute's heap
				D3D12_CONSTANT_BUFFER_VIEW_DESC cbvView;
				cbvView.BufferLocation = buffer->resource->GetGPUVirtualAddress();
				cbvView.SizeInBytes = (uint32_t)buffer->bufferSize;

				// Compute the slot on the heap
				D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(dx12_cs->cbvCPU);
				rtvHandle.ptr += (uint64_t)deviceI->descriptorSize[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] * slot;

				// Create the CBV
				deviceI->device->CreateConstantBufferView(&cbvView, rtvHandle);
			}

			void dispatch(CommandBuffer commandBuffer, ComputeShader computeShader, uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ)
			{
				DX12CommandBuffer* cmdI = (DX12CommandBuffer*)commandBuffer;
				DX12ComputeShader* dx12_cs = (DX12ComputeShader*)computeShader;

				// Bind the root descriptor tables
				ID3D12DescriptorHeap* ppHeaps[] = { dx12_cs->descriptorHeap};
				cmdI->cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
				cmdI->cmdList->SetComputeRootSignature(dx12_cs->rootSignature);
				cmdI->cmdList->SetComputeRootDescriptorTable(0, dx12_cs->srvGPU);
				cmdI->cmdList->SetComputeRootDescriptorTable(1, dx12_cs->uavGPU);
				cmdI->cmdList->SetComputeRootDescriptorTable(2, dx12_cs->cbvGPU);

				// Bind the shader and dispatch it
				cmdI->cmdList->SetPipelineState(dx12_cs->pipelineStateObject);
				cmdI->cmdList->Dispatch(sizeX, sizeY, sizeZ);
			}

			void enable_profiling_scope(CommandBuffer commandBuffer, ProfilingScope profilingScope)
			{
				DX12CommandBuffer* cmdI = (DX12CommandBuffer*)commandBuffer;
				DX12Query* query = (DX12Query*)profilingScope;
				cmdI->cmdList->EndQuery(query->heap, D3D12_QUERY_TYPE_TIMESTAMP, 0);
			}

			void disable_profiling_scope(CommandBuffer commandBuffer, ProfilingScope profilingScope)
			{
				DX12CommandBuffer* cmdI = (DX12CommandBuffer*)commandBuffer;
				DX12Query* query = (DX12Query*)profilingScope;
				cmdI->cmdList->EndQuery(query->heap, D3D12_QUERY_TYPE_TIMESTAMP, 1);
				cmdI->deviceI->device->SetStablePowerState(false);

				// Resolve the occlusion query and store the results in the query result buffer to be used on the subsequent frame.
				cmdI->cmdList->ResolveQueryData(query->heap, D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, query->result, 0);
			}
		}
	}
}
