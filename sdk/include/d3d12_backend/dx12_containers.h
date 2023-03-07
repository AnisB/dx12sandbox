#pragma once

// Bento includes
#include <bento_base/platform.h>
#include <bento_collection/vector.h>

// SDK includes
#include "gpu_backend/graphics_format.h"
#include "gpu_backend/graphics_buffer_type.h"
#include "gpu_backend/render_target_descriptor.h"

// DX12 includes
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <dxcapi.h>

namespace graphics_sandbox
{
	namespace d3d12
	{
		// Global DX12 Constants
		#define DX12_NUM_BACK_BUFFERS 2
		#define DX12_CONSTANT_BUFFER_ALIGNEMENT_SIZE 256

		// Declarations
		struct DX12Query;

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
			HANDLE fenceEvent;
			uint64_t fenceValue;
		};

		struct DX12CommandBuffer
		{
			DX12GraphicsDevice* deviceI;
			ID3D12CommandAllocator* cmdAlloc;
			ID3D12GraphicsCommandList* cmdList;
			uint32_t batchIdentifier;
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

		struct DX12DescriptorHeap
		{
			ID3D12DescriptorHeap* descriptorHeap;

			// GPU Handles for every resource type
			D3D12_GPU_DESCRIPTOR_HANDLE srvGPU;
			D3D12_GPU_DESCRIPTOR_HANDLE uavGPU;
			D3D12_GPU_DESCRIPTOR_HANDLE cbvGPU;

			// CPU Handles for every resource type
			D3D12_CPU_DESCRIPTOR_HANDLE srvCPU;
			D3D12_CPU_DESCRIPTOR_HANDLE uavCPU;
			D3D12_CPU_DESCRIPTOR_HANDLE cbvCPU;
		};

		struct DX12ComputeShader
		{
			ALLOCATOR_BASED;

			DX12ComputeShader(bento::IAllocator& allocator)
			: _allocator(allocator)
			, device(nullptr)
			, shaderBlob(nullptr)
			, rootSignature(nullptr)
			, pipelineStateObject(nullptr)
			, srvCount(0)
			, uavCount(0)
			, cbvCount(0)
			, srvIndex(0)
			, uavIndex(0)
			, cbvIndex(0)
			, cmdBatchIndex(0)
			, nextUsableHeap(0)
			, descriptorHeaps(allocator)
			{
			}

			// General
			DX12GraphicsDevice* device;
			IDxcBlob* shaderBlob;
			ID3D12RootSignature* rootSignature;
			ID3D12PipelineState* pipelineStateObject;

			// Number of resources
			uint32_t srvCount;
			uint32_t uavCount;
			uint32_t cbvCount;

			// Resources indices
			uint32_t srvIndex;
			uint32_t uavIndex;
			uint32_t cbvIndex;

			// Set of descriptor heaps that can be used for this
			uint32_t cmdBatchIndex;
			uint32_t nextUsableHeap;
			bento::Vector<DX12DescriptorHeap> descriptorHeaps;
			bento::IAllocator& _allocator;
		};

		struct DX12GraphicsBuffer
		{
			ID3D12Resource* resource;
			D3D12_RESOURCE_STATES state;
			uint64_t bufferSize;
			uint32_t elementSize;
			GraphicsBufferType type;
		};

		struct DX12Query
		{
			ID3D12QueryHeap* heap;
			ID3D12Resource* result;
			D3D12_RESOURCE_STATES state;
			uint64_t frequency;
		};

		// Conversion methods
		DXGI_FORMAT graphics_format_to_dxgi_format(GraphicsFormat graphicsFormat);
		D3D12_RESOURCE_DIMENSION texture_dimension_to_dx12_resource_dimension(TextureDimension textureDimension);
	}
}