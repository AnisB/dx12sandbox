// Windows include
#include <Windows.h>
#include <ctime>
#include <chrono>

// Bento includes
#include <bento_math/vector4.h>

// Graphics API include
#include "gpu_backend/dx12_backend.h"

using namespace graphics_sandbox;
using namespace graphics_sandbox::d3d12;

// Global structures
CommandQueue g_commandQueue = 0;
CommandBuffer g_commandBuffer = 0;
SwapChain g_swapChain = 0;

void Render()
{
	// Reset the command buffer
	command_buffer::reset(g_commandBuffer);

	// Grab the current swap chain render target
	RenderTarget renderTarget = swap_chain::get_current_render_target(g_swapChain);

	// Get time

	// Int that is going to be used for the color picking
	uint64_t now = time(0) * 20;

	// notice the 256's instead of 255
	float r, g, b;
	r = (now % 256) / 255.0f;
	g = ((now / 256) % 256) / 255.0f;
	b = ((now / (256 * 256)) % 256) / 255.0f;

	// Clear the render target
	command_buffer::clear_render_target(g_commandBuffer, renderTarget, bento::vector4(r, g, b, 1.0));

	// Set the render target in present mode
	command_buffer::render_target_present(g_commandBuffer, renderTarget);

	// Close the command buffer
	command_buffer::close(g_commandBuffer);

	// Execute the command buffer in the command queue
	command_queue::execute_command_buffer(g_commandQueue, g_commandBuffer);

	// Present
	swap_chain::present(g_swapChain, g_commandQueue);
}

int CALLBACK main(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
	// Define the initial settings
    TGraphicSettings settings = d3d12::default_settings();
    settings.window_name = "DX12 Window";
    settings.data[0] = (uint64_t)hInstance;

	// Create the window
	RenderWindow window = window::create_window(settings);

	// Create the graphics device
	GraphicsDevice graphicsDevice = graphics_device::create_graphics_device();

	// Create the command queue
	g_commandQueue = command_queue::create_command_queue(graphicsDevice);

	// Create the swap chain
	g_swapChain = swap_chain::create_swap_chain(window, graphicsDevice, g_commandQueue);

    // Create the command buffer
	g_commandBuffer = d3d12::command_buffer::create_command_buffer(graphicsDevice);

    // Show the window
    d3d12::window::show(window);

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

    // Destroy the command buffer
    command_buffer::destroy_command_buffer(g_commandBuffer);

	// Destroy the swap chain
	swap_chain::destroy_swap_chain(g_swapChain);

	// Destroy the command queue
	command_queue::destroy_command_queue(g_commandQueue);

    // Destroy the graphics device
    graphics_device::destroy_graphics_device(graphicsDevice);

    return 0;
}