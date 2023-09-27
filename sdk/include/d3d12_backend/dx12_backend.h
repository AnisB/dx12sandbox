#pragma once
#ifdef D3D12_SUPPORTED

// bento includes
#include <bento_math/types.h>

// Library includes
#include "gpu_backend/gpu_types.h"
#include "gpu_backend/settings.h"
#include "gpu_backend/render_target_descriptor.h"
#include "gpu_backend/compute_shader_descriptor.h"
#include "gpu_backend/graphics_buffer_type.h"
#include "gpu_backend/constant_buffer_type.h"

namespace graphics_sandbox
{
    namespace d3d12
    {
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
            GraphicsDevice create_graphics_device(bool enableDebug = false, uint32_t preferred_adapter = UINT32_MAX, bool stable_power_state = false);
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
            void flush(CommandQueue commandQueue);
        }

        // Swap Chain API
        namespace swap_chain
        {
            // Creation and Destruction
            SwapChain create_swap_chain(RenderWindow window, GraphicsDevice graphicsDevice, CommandQueue commandQueue);
            void destroy_swap_chain(SwapChain swapChain);

            // Operations
            RenderTexture get_current_render_texture(SwapChain swapChain);
            void present(SwapChain swapChain, CommandQueue commandQueue);
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
            void set_render_texture(CommandBuffer commandBuffer, RenderTexture renderTexture);
            void clear_render_texture(CommandBuffer commandBuffer, RenderTexture renderTexture, const bento::Vector4& color);
            void render_texture_present(CommandBuffer commandBuffer, RenderTexture renderTexture);
            void copy_graphics_buffer(CommandBuffer commandBuffer, GraphicsBuffer inputBuffer, GraphicsBuffer outputBuffer);
            void copy_constant_buffer(CommandBuffer commandBuffer, ConstantBuffer inputBuffer, ConstantBuffer outputBuffer);
            void uav_barrier(CommandBuffer commandBuffer, GraphicsBuffer targetBuffer);

            // Compute operations
            void set_compute_graphics_buffer_uav(CommandBuffer commandBuffer, ComputeShader computeShader, uint32_t slot, GraphicsBuffer graphicsBuffer);
            void set_compute_graphics_buffer_srv(CommandBuffer commandBuffer, ComputeShader computeShader, uint32_t slot, GraphicsBuffer graphicsBuffer);
            void set_compute_graphics_buffer_cbv(CommandBuffer commandBuffer, ComputeShader computeShader, uint32_t slot, ConstantBuffer constantBuffer);
            void dispatch(CommandBuffer commandBuffer, ComputeShader computeShader, uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ);
            
            // Profiling
            void enable_profiling_scope(CommandBuffer commandBuffer, ProfilingScope scope);
            void disable_profiling_scope(CommandBuffer commandBuffer, ProfilingScope scope);
        }

        namespace graphics_resources
        {
            // Render texture
            RenderTexture create_render_texture(GraphicsDevice graphicsDevice, RenderTextureDescriptor rtDesc);
            void destroy_render_texture(RenderTexture renderTarget);

            // Graphics Buffers
            GraphicsBuffer create_graphics_buffer(GraphicsDevice graphicsDevice, uint64_t bufferSize, uint32_t elementSize, GraphicsBufferType bufferType);
            void destroy_graphics_buffer(GraphicsBuffer graphicsBuffer);
            void set_data(GraphicsBuffer graphicsBuffer, char* buffer, uint64_t bufferSize);
            char* allocate_cpu_buffer(GraphicsBuffer graphicsBuffer);
            void release_cpu_buffer(GraphicsBuffer graphicsBuffer);

            // Constant Buffers
            ConstantBuffer create_constant_buffer(GraphicsDevice graphicsDevice, uint64_t bufferSize, uint32_t elementSize, ConstantBufferType bufferType);
            void destroy_constant_buffer(ConstantBuffer constantBuffer);
            void upload_constant_buffer(ConstantBuffer constantBuffer, const char* bufferData, uint32_t bufferSize);
        }

        namespace descriptor_heap
        {
            DescriptorHeap create_descriptor_heap(GraphicsDevice device, uint32_t numDescriptors, uint32_t type);
            void destroy_descriptor_heap(DescriptorHeap descriptorHeap);
        }

        namespace compute_shader
        {
            ComputeShader create_compute_shader(GraphicsDevice graphicsDevice, const ComputeShaderDescriptor& computeShaderDescriptor);
            void destroy_compute_shader(ComputeShader computeShader);
        }

        namespace profiling_scope
        {
            ProfilingScope create_profiling_scope(GraphicsDevice graphicsDevice, CommandQueue commandQueue);
            void destroy_profiling_scope(ProfilingScope profilingScope);
            uint64_t get_duration_us(ProfilingScope profilingScope);
        }
    }
}
#endif