#pragma once
#ifdef D3D12_SUPPORTED

// bento includes
#include <bento_math/types.h>

// Library includes
#include "gpu_backend/gpu_types.h"
#include "gpu_backend/settings.h"
#include "gpu_backend/render_target_descriptor.h"

namespace graphics_sandbox
{
	namespace d3d12
	{
		TGraphicSettings default_settings();

		// Window API
		namespace window
		{
			// Creation and destruction
			RenderWindow create_window(const TGraphicSettings& graphicSettings);
			void destroy_window(RenderWindow renderWindow);

			// Manipulation
			void show(RenderWindow renderWindow);
			void hide(RenderWindow renderWindow);
		}

		// Graphics Device API
		namespace graphics_device
		{
			GraphicsDevice create_graphics_device();
			void destroy_graphics_device(GraphicsDevice graphicsDevice);
		}

		// Command Queue API
		namespace command_queue
		{
			// Creation and destruction
			CommandQueue create_command_queue(GraphicsDevice graphicsDevice);
			void destroy_command_queue(CommandQueue commandQueue);

			// Operation
			void execute_command_buffer(CommandQueue commandQueue, CommandBuffer commandBuffer);
		}

		// Swap Chain API
		namespace swap_chain
		{
			// Creation and Destruction
			SwapChain create_swap_chain(RenderWindow window, GraphicsDevice graphicsDevice, CommandQueue commandQueue);
			void destroy_swap_chain(SwapChain swapChain);

			// Operations
			RenderTarget get_current_render_target(SwapChain swapChain);
			void present(SwapChain swapChain, CommandQueue commandQueue);
		}

		// Descriptor Heap API
		namespace descriptor_heap
		{
			// Creation and Destruction
			DescriptorHeap create_descriptor_heap(GraphicsDevice graphicsDevice, uint32_t numDescriptors, bool isUAV, bool isDepthStencil);
			void destroy_descriptor_heap(DescriptorHeap descriptorHeap);
		}

		// Fence API
		namespace fence
		{
			// Creation and Destruction
			Fence create_fence(GraphicsDevice graphicsDevice);
			void destroy_fence(Fence fence);
			FenceEvent create_fence_event();
			void destroy_fence_event(FenceEvent fenceEvent);

			// Operations
			void wait_for_fence_value(Fence fence, uint64_t fenceValue, FenceEvent fenceEvent, uint64_t maxTime);
		}

		// Command Buffer API
		namespace command_buffer
		{
			// Creation and Destruction
			CommandBuffer create_command_buffer(GraphicsDevice graphicsDevice);
			void destroy_command_buffer(CommandBuffer command_buffer);

			// Operations
			void reset(CommandBuffer commandBuffer);
			void close(CommandBuffer commandBuffer);
			void set_render_target(CommandBuffer commandBuffer, RenderTarget renderTarget);
			void clear_render_target(CommandBuffer commandBuffer, RenderTarget renderTarget, const bento::Vector4& color);
			void render_target_present(CommandBuffer commandBuffer, RenderTarget renderTarget);
		}

		namespace render_target
		{
			RenderTarget create_render_target(GraphicsDevice graphicsDevice, RenderTextureDescriptor rtDesc);
			void destroy_render_target(RenderTarget renderTarget);
		}

		namespace compute_shader
		{
			ComputeShader create_compute_shader(GraphicsDevice graphicsDevice);
		}
	}
}
#endif