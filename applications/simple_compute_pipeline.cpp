// Windows include
#include <Windows.h>
#include <ctime>
#include <chrono>

// Bento includes
#include <bento_base/security.h>
#include <bento_math/vector4.h>
#include <bento_collection/vector.h>

// Graphics API include
#include "gpu_backend/dx12_backend.h"
#include "gpu_backend/event_collector.h"

using namespace graphics_sandbox;
using namespace graphics_sandbox::d3d12;

int CALLBACK main(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
	// Create the graphics device
	GraphicsDevice graphicsDevice = graphics_device::create_graphics_device();

	// Create the command queue
	CommandQueue commandQueue = command_queue::create_command_queue(graphicsDevice);

	// Create the command buffer
	CommandBuffer commandBuffer = d3d12::command_buffer::create_command_buffer(graphicsDevice);

	// Grab the allocator
	bento::IAllocator* allocator = bento::common_allocator();
	assert(allocator != nullptr);

	// Create the input buffer
	const uint32_t numElements = 4096;
	bento::Vector<float> inputBuffer(*allocator, numElements);

	// Create the input graphics buffer
	GraphicsBuffer inputBuffer = d3d12::graphics_resources::create_graphics_buffer(graphicsDevice, (const char*)inputBuffer.begin(), sizeof(float) * numElements);

	// Create the output graphics buffer
	GraphicsBuffer outputBuffer = d3d12::graphics_resources::create_graphics_buffer(graphicsDevice, nullptr, sizeof(float) * numElements);

	// Create the compute pipeline state object

	// Reset the command buffer
	command_buffer::reset(commandBuffer);

	// Close the command buffer
	command_buffer::close(commandBuffer);

	// Execute the command buffer in the command queue
	command_queue::execute_command_buffer(commandQueue, commandBuffer);

	// Destroy the command buffer
	command_buffer::destroy_command_buffer(commandBuffer);

	// Destroy the command queue
	command_queue::destroy_command_queue(commandQueue);

	// Destroy the graphics device
	graphics_device::destroy_graphics_device(graphicsDevice);

	return 0;
}