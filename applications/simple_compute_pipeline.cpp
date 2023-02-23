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
const uint32_t numElements = 128;
const uint32_t workGroupSize = 64;

// Constnat buffer
struct SimpleCB
{
	float data0;
	float data1;
	float data2;
	float data3;
};

int CALLBACK main(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
	// Create the graphics device
	GraphicsDevice graphicsDevice = graphics_device::create_graphics_device(true);

	// Compile and create the compute shader
	ComputeShaderDescriptor csd(*bento::common_allocator());
	csd.filename = shader_file_name;
	csd.kernelname = shader_kernel_name;
	csd.srvCount = 2;
	csd.uavCount = 2;
	csd.cbvCount = 1;
	ComputeShader computeShader = compute_shader::create_compute_shader(graphicsDevice, csd);

	// Create the command queue
	CommandQueue commandQueue = command_queue::create_command_queue(graphicsDevice);

	// Create the command buffer
	CommandBuffer commandBuffer = d3d12::command_buffer::create_command_buffer(graphicsDevice);

	// Create the required graphics buffers
	GraphicsBuffer uploadBuffer0 = graphics_resources::create_graphics_buffer(graphicsDevice, sizeof(uint32_t) * numElements, 4, GraphicsBufferType::Upload);
	GraphicsBuffer inputBuffer0 = graphics_resources::create_graphics_buffer(graphicsDevice, sizeof(uint32_t) * numElements, 4, GraphicsBufferType::Default);
	GraphicsBuffer uploadBuffer1 = graphics_resources::create_graphics_buffer(graphicsDevice, sizeof(uint32_t) * numElements, 4, GraphicsBufferType::Upload);
	GraphicsBuffer inputBuffer1 = graphics_resources::create_graphics_buffer(graphicsDevice, sizeof(uint32_t) * numElements, 4, GraphicsBufferType::Default);
	GraphicsBuffer outputBuffer0 = graphics_resources::create_graphics_buffer(graphicsDevice, sizeof(uint32_t) * numElements * 4, 4, GraphicsBufferType::Default);
	GraphicsBuffer readbackBuffer0 = graphics_resources::create_graphics_buffer(graphicsDevice, sizeof(uint32_t) * numElements * 4, 4, GraphicsBufferType::Readback);
	GraphicsBuffer outputBuffer1 = graphics_resources::create_graphics_buffer(graphicsDevice, sizeof(uint32_t) * numElements * 4, 4, GraphicsBufferType::Default);
	GraphicsBuffer readbackBuffer1 = graphics_resources::create_graphics_buffer(graphicsDevice, sizeof(uint32_t) * numElements * 4, 4, GraphicsBufferType::Readback);

	// Fill the first input buffer
	bento::Vector<uint32_t> inputBuffer0CPU(*bento::common_allocator(), numElements);
	for (int i = 0; i < numElements; ++i)
		inputBuffer0CPU[i] = i;
	graphics_resources::set_data(uploadBuffer0, (char*)inputBuffer0CPU.begin(), numElements * sizeof(uint32_t));

	// Fill the second input buffer
	bento::Vector<uint32_t> inputBuffer1CPU(*bento::common_allocator(), numElements);
	for (int i = 0; i < numElements; ++i)
		inputBuffer1CPU[i] = i * 2;
	graphics_resources::set_data(uploadBuffer1, (char*)inputBuffer1CPU.begin(), numElements * sizeof(uint32_t));

	// Create the constant buffer
	SimpleCB constantBufferCPU = { 2, 3, 4, 5 };
	ConstantBuffer constantBuffer = graphics_resources::create_constant_buffer(graphicsDevice, sizeof(bento::Vector4) * 1, 1);
	graphics_resources::upload_constant_buffer(constantBuffer, (const char*)&constantBufferCPU, sizeof(SimpleCB));

	// Reset the command buffer
	command_buffer::reset(commandBuffer);

	// Copy the upload buffer into the input buffer
	command_buffer::copy_graphics_buffer(commandBuffer, uploadBuffer0, inputBuffer0);
	command_buffer::copy_graphics_buffer(commandBuffer, uploadBuffer1, inputBuffer1);

	// Dispatch the Compute shader
	command_buffer::set_compute_graphics_buffer_cbv(commandBuffer, computeShader, 0, constantBuffer);
	command_buffer::set_compute_graphics_buffer_srv(commandBuffer, computeShader, 0, inputBuffer0);
	command_buffer::set_compute_graphics_buffer_srv(commandBuffer, computeShader, 1, inputBuffer1);
	command_buffer::set_compute_graphics_buffer_uav(commandBuffer, computeShader, 0, outputBuffer0);
	command_buffer::set_compute_graphics_buffer_uav(commandBuffer, computeShader, 1, outputBuffer1);
	command_buffer::dispatch(commandBuffer, computeShader, numElements / workGroupSize, 1, 1);

	// Copy the output into the readback buffer
	command_buffer::copy_graphics_buffer(commandBuffer, outputBuffer0, readbackBuffer0);
	command_buffer::copy_graphics_buffer(commandBuffer, outputBuffer1, readbackBuffer1);

	// Close the command buffer
	command_buffer::close(commandBuffer);

	// Execute the command buffer in the command queue
	command_queue::execute_command_buffer(commandQueue, commandBuffer);

	// Flush the queue
	command_queue::flush(commandQueue);

	// Create a cpu view on the readback buffer
	std::cout << "First Buffer" << std::endl;
	uint32_t* outputData = (uint32_t*) graphics_resources::allocate_cpu_buffer(readbackBuffer0);
	for (uint32_t idx = 0; idx < numElements; ++idx)
	{
		assert_msg(outputData[4 * idx] == idx, "Failure 0");
		assert_msg(outputData[4 * idx + 1] == 7, "Failure 1");
		assert_msg(outputData[4 * idx + 3] == 5, "Failure 2");
		//std::cout << outputData[4 * idx] << ", " << outputData[4 * idx + 1] << ", " << outputData[4 * idx + 2] << ", " << outputData[4 * idx + 3] << std::endl;
	}
	graphics_resources::release_cpu_buffer(readbackBuffer0);

	std::cout << "Second Buffer" << std::endl;
	outputData = (uint32_t*)graphics_resources::allocate_cpu_buffer(readbackBuffer1);
	for (uint32_t idx = 0; idx < numElements; ++idx)
	{
		assert_msg(outputData[4 * idx] == idx * 2, "Failure 0");
		assert_msg(outputData[4 * idx + 1] == 9, "Failure 1");
		assert_msg(outputData[4 * idx + 3] == 3, "Failure 2");
		//std::cout << outputData[4 * idx] << ", " << outputData[4 * idx + 1] << ", " << outputData[4 * idx + 2] << ", " << outputData[4 * idx + 3] << std::endl;
	}
	graphics_resources::release_cpu_buffer(readbackBuffer1);

	// Release the grpahics buffer
	graphics_resources::destroy_graphics_buffer(readbackBuffer1);
	graphics_resources::destroy_graphics_buffer(outputBuffer1);
	graphics_resources::destroy_graphics_buffer(readbackBuffer0);
	graphics_resources::destroy_graphics_buffer(outputBuffer0);
	graphics_resources::destroy_graphics_buffer(inputBuffer1);
	graphics_resources::destroy_graphics_buffer(uploadBuffer1);
	graphics_resources::destroy_graphics_buffer(inputBuffer0);
	graphics_resources::destroy_graphics_buffer(uploadBuffer0);

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