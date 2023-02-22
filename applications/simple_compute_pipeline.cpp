// Windows include
#include <Windows.h>
#include <ctime>
#include <iostream>

// Bento includes
#include <bento_base/security.h>
#include <bento_math/vector4.h>
#include <bento_collection/vector.h>
#include <bento_collection/dynamic_string.h>

// Graphics API include
#include "gpu_backend/dx12_backend.h"
#include "gpu_backend/event_collector.h"

using namespace graphics_sandbox;
using namespace graphics_sandbox::d3d12;

// Compute shader that we will be executing
const char* shader_file_name = "C:/Temp/compute_shader.cso";
const char* shader_kernel_name = "SquareKernel";
const uint32_t numElements = 4096;
const uint32_t workGroupSize = 64;

int CALLBACK main(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
	// Create the graphics device
	GraphicsDevice graphicsDevice = graphics_device::create_graphics_device();

	// Compile and create the compute shader
	ComputeShaderDescriptor csd(*bento::common_allocator());
	csd.filename = shader_file_name;
	csd.kernelname = shader_kernel_name;
	csd.srvCount = 1;
	csd.uavCount = 1;
	csd.cbvCount = 0;
	ComputeShader computeShader = compute_shader::create_compute_shader(graphicsDevice, csd);

	// Create the command queue
	CommandQueue commandQueue = command_queue::create_command_queue(graphicsDevice);

	// Create the command buffer
	CommandBuffer commandBuffer = d3d12::command_buffer::create_command_buffer(graphicsDevice);

	// Create the required graphics buffers
	GraphicsBuffer uploadBuffer = graphics_resources::create_graphics_buffer(graphicsDevice, sizeof(uint32_t) * numElements, 4, GraphicsBufferType::Upload);
	GraphicsBuffer inputBuffer = graphics_resources::create_graphics_buffer(graphicsDevice, sizeof(uint32_t) * numElements, 4, GraphicsBufferType::Default);
	GraphicsBuffer outputBuffer = graphics_resources::create_graphics_buffer(graphicsDevice, sizeof(uint32_t) * numElements * 4, 4, GraphicsBufferType::Default);
	GraphicsBuffer readbackBuffer = graphics_resources::create_graphics_buffer(graphicsDevice, sizeof(uint32_t) * numElements * 4, 4, GraphicsBufferType::Readback);

	// Reset the command buffer
	command_buffer::reset(commandBuffer);

	// Create the input buffer, initialize it and upload to the GPU
	bento::Vector<uint32_t> inputBufferCPU(*bento::common_allocator(), numElements);
	for (int i = 0; i < numElements; ++i)
		inputBufferCPU[i] = i;
	graphics_resources::set_data(uploadBuffer, (char*)inputBufferCPU.begin(), numElements * sizeof(uint32_t));

	// Copy the upload buffer into the input buffer
	command_buffer::copy_graphics_buffer(commandBuffer, uploadBuffer, inputBuffer);

	// Dispatch the Compute shader
	command_buffer::set_compute_graphics_buffer_srv(commandBuffer, computeShader, 0, inputBuffer);
	command_buffer::set_compute_graphics_buffer_uav(commandBuffer, computeShader, 0, outputBuffer);
	command_buffer::dispatch(commandBuffer, computeShader, numElements / workGroupSize, 1, 1);

	// Copy the output into the readback buffer
	command_buffer::copy_graphics_buffer(commandBuffer, outputBuffer, readbackBuffer);

	// Close the command buffer
	command_buffer::close(commandBuffer);

	// Execute the command buffer in the command queue
	command_queue::execute_command_buffer(commandQueue, commandBuffer);

	// Flush the queue
	command_queue::flush(commandQueue);

	// Create a cpu view on the readback buffer
	uint32_t* outputData = (uint32_t*) graphics_resources::allocate_cpu_buffer(readbackBuffer);
	for (uint32_t idx = 0; idx < numElements; ++idx)
	{
		std::cout << outputData[4 * idx] << ", " << outputData[4 * idx + 1] << ", " << outputData[4 * idx + 2] << ", " << outputData[4 * idx + 3] << std::endl;
	}

	graphics_resources::release_cpu_buffer(readbackBuffer);

	// Destroy the compute shader
	compute_shader::destroy_compute_shader(computeShader);

	// Destroy the command buffer
	command_buffer::destroy_command_buffer(commandBuffer);

	// Destroy the command queue
	command_queue::destroy_command_queue(commandQueue);

	// Destroy the graphics device
	graphics_device::destroy_graphics_device(graphicsDevice);

	return 0;
}