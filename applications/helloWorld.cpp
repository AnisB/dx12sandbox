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

    // Create the command buffer
    CommandBuffer cmd = d3d12::command_buffer::create_command_buffer(renderEnvironment);

    // Create a fence
    Fence fence = d3d12::graphics_fence::create_fence(renderEnvironment);
    FenceEvent fenceEvent = d3d12::graphics_fence::create_fence_event();

    // Grab the render window
    RenderWindow renderWindow = d3d12::render_system::render_window(renderEnvironment);

    // Show the window
    d3d12::window::show(renderWindow);

    // Render loop
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // Make sure the command queue has finished all commands before closing.
    Flush(g_CommandQueue, g_Fence, g_FenceValue, g_FenceEvent);

    // destroy fence event
    d3d12::graphics_fence::destroy_fence_event(fenceEvent);

    // Destroy the fence
    d3d12::graphics_fence::destroy_fence(fence);

    // Destroy the command buffer
    d3d12::command_buffer::destroy_command_buffer(cmd);

    // Destroy the render environment
    d3d12::render_system::destroy_render_environment(renderEnvironment);

    return 0;
}