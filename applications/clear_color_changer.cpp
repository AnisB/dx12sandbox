// Windows include
#include <Windows.h>
#include <ctime>
#include <chrono>

// Bento includes
#include <bento_math/vector4.h>

// Graphics API include
#include "gpu_backend/dx12_backend.h"
#include "gpu_backend/event_collector.h"

using namespace graphics_sandbox;
using namespace graphics_sandbox::d3d12;

void Render(CommandQueue commandQueue, CommandBuffer commandBuffer, SwapChain swapChain)
{
	// Reset the command buffer
	command_buffer::reset(commandBuffer);

	// Grab the current swap chain render target
	RenderTexture renderTexture = swap_chain::get_current_render_texture(swapChain);

	// Int that is going to be used for the color picking
	uint64_t now = time(0) * 20;

	// notice the 256's instead of 255
	float r, g, b;
	r = (now % 256) / 255.0f;
	g = ((now / 256) % 256) / 255.0f;
	b = ((now / (256 * 256)) % 256) / 255.0f;

	// Clear the render target
	command_buffer::clear_render_texture(commandBuffer, renderTexture, bento::vector4(r, g, b, 1.0));

	// Set the render target in present mode
	command_buffer::render_texture_present(commandBuffer, renderTexture);

	// Close the command buffer
	command_buffer::close(commandBuffer);

	// Execute the command buffer in the command queue
	command_queue::execute_command_buffer(commandQueue, commandBuffer);

	// Present
	swap_chain::present(swapChain, commandQueue);
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
	CommandQueue commandQueue = command_queue::create_command_queue(graphicsDevice);

	// Create the swap chain
	SwapChain swapChain = swap_chain::create_swap_chain(window, graphicsDevice, commandQueue);

    // Create the command buffer
	CommandBuffer commandBuffer = d3d12::command_buffer::create_command_buffer(graphicsDevice);

    // Show the window
    d3d12::window::show(window);

    // Render loop
	bool activeLoop = true;
    while (activeLoop)
    {
		FrameEvent frameEvent;
		if (event_collector::peek_event(frameEvent))
		{
			switch (frameEvent)
			{
				case FrameEvent::Paint:
				{
					Render(commandQueue, commandBuffer, swapChain);
				}
				break;
				case FrameEvent::Close:
				{
					activeLoop = false;
				}
				break;
				case FrameEvent::Destroy:
				{
					activeLoop = false;
				}
				break;
			}
		}

		// Need to figure out wtf this is reuquired."
		MSG msg = {};
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
    }

    // Destroy the command buffer
    command_buffer::destroy_command_buffer(commandBuffer);

	// Destroy the swap chain
	swap_chain::destroy_swap_chain(swapChain);

	// Destroy the command queue
	command_queue::destroy_command_queue(commandQueue);

    // Destroy the graphics device
    graphics_device::destroy_graphics_device(graphicsDevice);

	// Destroy the window
	window::destroy_window(window);

    return 0;
}