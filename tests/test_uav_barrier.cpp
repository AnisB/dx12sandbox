// Windows include
#include <Windows.h>
#include <iostream>
#include <vector>

// Bento includes
#include <bento_base/security.h>

// Graphics API include
#include "d3d12_backend/dx12_backend.h"
#include "gpu_backend/event_collector.h"

using namespace graphics_sandbox;
using namespace graphics_sandbox::d3d12;

// Compute shader that we will be executing
const char* shader_file_name = "IncrementBuffer.compute";
const char* shader_kernel_name = "IncrementBuffer";
const uint32_t workGroupSize = 32;
const uint32_t numElements = 1024;
const uint32_t numIterations = 16;

// Constnat buffer
struct SimpleCB
{
    uint32_t data0;
    uint32_t data1;
    uint32_t data2;
    uint32_t data3;
};

int CALLBACK main(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
    // The root directory was not specified in this case
    if (__argc < 2)
    {
        printf("[ERROR] Repository path not specified\n");
        return -1;
    }

    // Create the graphics device
    GraphicsDevice graphicsDevice = graphics_device::create_graphics_device(true);

    // Location of the shader library
    bento::DynamicString shaderLibrary(*bento::common_allocator(), __argv[1]);
    shaderLibrary += "\\shaders";

    // Compile and create the compute shader
    ComputeShaderDescriptor csd(*bento::common_allocator());
    csd.filename = shaderLibrary;
    csd.filename += "\\";
    csd.filename += shader_file_name;
    csd.kernelname = shader_kernel_name;
    csd.srvCount = 0;
    csd.uavCount = 1;
    csd.cbvCount = 1;
    ComputeShader computeShader = compute_shader::create_compute_shader(graphicsDevice, csd);

    // Create the command queue
    CommandQueue commandQueue = command_queue::create_command_queue(graphicsDevice);

    // Create the command buffer
    CommandBuffer commandBuffer = d3d12::command_buffer::create_command_buffer(graphicsDevice);

    // Create the required graphics buffers
    GraphicsBuffer uploadBuffer0 = graphics_resources::create_graphics_buffer(graphicsDevice, sizeof(uint32_t) * numElements, sizeof(uint32_t), GraphicsBufferType::Upload);
    GraphicsBuffer buffer0 = graphics_resources::create_graphics_buffer(graphicsDevice, sizeof(uint32_t) * numElements, sizeof(uint32_t), GraphicsBufferType::Default);
    GraphicsBuffer readbackBuffer0 = graphics_resources::create_graphics_buffer(graphicsDevice, sizeof(uint32_t) * numElements, sizeof(uint32_t), GraphicsBufferType::Readback);
    GraphicsBuffer uploadBuffer1 = graphics_resources::create_graphics_buffer(graphicsDevice, sizeof(uint32_t) * numElements, sizeof(uint32_t), GraphicsBufferType::Upload);
    GraphicsBuffer buffer1 = graphics_resources::create_graphics_buffer(graphicsDevice, sizeof(uint32_t) * numElements, sizeof(uint32_t), GraphicsBufferType::Default);
    GraphicsBuffer readbackBuffer1 = graphics_resources::create_graphics_buffer(graphicsDevice, sizeof(uint32_t) * numElements, sizeof(uint32_t), GraphicsBufferType::Readback);

    // Fill the first input buffer
    std::vector<uint32_t> inputBufferCPU0(numElements);
    std::vector<uint32_t> inputBufferCPU1(numElements);
    for (uint32_t i = 0; i < numElements; ++i)
    {
        inputBufferCPU0[i] = i;
        inputBufferCPU1[i] =  2 * i;
    }
    graphics_resources::set_data(uploadBuffer0, (char*)inputBufferCPU0.data(), numElements * sizeof(uint32_t));
    graphics_resources::set_data(uploadBuffer1, (char*)inputBufferCPU1.data(), numElements * sizeof(uint32_t));

    // Create all the constant buffers
    std::vector<ConstantBuffer> constantBufferArray;
    for (uint32_t iter = 0; iter < numIterations; ++iter)
    {
        ConstantBuffer constantBufferUpload = graphics_resources::create_constant_buffer(graphicsDevice, sizeof(SimpleCB), sizeof(SimpleCB), ConstantBufferType::Static);
        SimpleCB constantBufferCPU = { iter, 0, 0, 0 };
        graphics_resources::upload_constant_buffer(constantBufferUpload, (const char*)&constantBufferCPU, sizeof(SimpleCB));
        constantBufferArray.push_back(constantBufferUpload);
    }
    ConstantBuffer constantBufferRuntime = graphics_resources::create_constant_buffer(graphicsDevice, sizeof(SimpleCB), sizeof(SimpleCB), ConstantBufferType::Default);

    // Reset the command buffer
    command_buffer::reset(commandBuffer);
    command_buffer::copy_graphics_buffer(commandBuffer, uploadBuffer0, buffer0);
    command_buffer::copy_graphics_buffer(commandBuffer, uploadBuffer1, buffer1);

    for (uint32_t iter = 0; iter < numIterations; ++iter)
    {
        command_buffer::copy_constant_buffer(commandBuffer, constantBufferArray[iter], constantBufferRuntime);

        // Dispatch the Compute shader on the first buffer
        command_buffer::set_compute_graphics_buffer_cbv(commandBuffer, computeShader, 0, constantBufferRuntime);
        command_buffer::set_compute_graphics_buffer_uav(commandBuffer, computeShader, 0, buffer0);
        command_buffer::dispatch(commandBuffer, computeShader, numElements / workGroupSize, 1, 1);
        command_buffer::uav_barrier(commandBuffer, buffer0);

        // Dispatch the Compute shader on the second buffer
        command_buffer::set_compute_graphics_buffer_cbv(commandBuffer, computeShader, 0, constantBufferRuntime);
        command_buffer::set_compute_graphics_buffer_uav(commandBuffer, computeShader, 0, buffer1);
        command_buffer::dispatch(commandBuffer, computeShader, numElements / workGroupSize, 1, 1);
        command_buffer::uav_barrier(commandBuffer, buffer1);
    }

    // Copy the output into the readback buffer
    command_buffer::copy_graphics_buffer(commandBuffer, buffer0, readbackBuffer0);
    command_buffer::copy_graphics_buffer(commandBuffer, buffer1, readbackBuffer1);

    // Close the command buffer
    command_buffer::close(commandBuffer);

    // Execute the command buffer in the command queue
    command_queue::execute_command_buffer(commandQueue, commandBuffer);

    // Flush the queue
    command_queue::flush(commandQueue);

    // Expected added value
    uint32_t totalValue = 0;
    for (uint32_t i = 0; i < numIterations; ++i)
        totalValue += i;

    // Create a cpu view on the readback buffer
    uint32_t* outputData0 = (uint32_t*) graphics_resources::allocate_cpu_buffer(readbackBuffer0);
    for (uint32_t idx = 0; idx < numElements; ++idx)
        assert(outputData0[idx] == (idx + totalValue));
    graphics_resources::release_cpu_buffer(readbackBuffer0);

    // Create a cpu view on the readback buffer
    uint32_t* outputData1 = (uint32_t*)graphics_resources::allocate_cpu_buffer(readbackBuffer1);
    for (uint32_t idx = 0; idx < numElements; ++idx)
        assert(outputData1[idx] == (2 * idx + totalValue));
    graphics_resources::release_cpu_buffer(readbackBuffer1);

    // Destroy all the constant buffers
    for (uint32_t iter = 0; iter < numIterations; ++iter)
        graphics_resources::destroy_constant_buffer(constantBufferArray[iter]);

    // Release the grpahics buffer
    graphics_resources::destroy_graphics_buffer(readbackBuffer1);
    graphics_resources::destroy_graphics_buffer(buffer1);
    graphics_resources::destroy_graphics_buffer(uploadBuffer1);
    graphics_resources::destroy_graphics_buffer(readbackBuffer0);
    graphics_resources::destroy_graphics_buffer(buffer0);
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