#include <Windows.h>

#include "gpu_backend/dx12_backend.h"

using namespace graphics_sandbox;

int CALLBACK main(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
    // Create the render environement
    TGraphicSettings settings = d3d12::default_settings();
    settings.window_name = "DX12 Window";
    settings.data[0] = (uint64_t)hInstance;
    RenderEnvironment renderEnvironment = d3d12::render_system::create_render_environment(settings);

    // Grab the render window
    RenderWindow renderWindow = d3d12::render_system::render_window(renderEnvironment);

    // Show the window
    d3d12::window::show(renderWindow);

    // Create the command buffer
    CommandBuffer cmd = d3d12::command_buffer::create_command_buffer(renderEnvironment);

    // Create a fence
    Fence fence = d3d12::graphics_fence::create_fence(renderEnvironment);

    // Destroy the fence
    d3d12::graphics_fence::destroy_fence(fence);

    // Destroy the command buffer
    d3d12::command_buffer::destroy_command_buffer(cmd);

    // Destroy the render environment
    d3d12::render_system::destroy_render_environment(renderEnvironment);

    return 0;
}