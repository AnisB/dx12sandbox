// Bento includes
#include <bento_memory/system_allocator.h>
#include <bento_memory/common.h>

// Internal includes
#include "gpu_backend_c_api.h"
#include "d3d12_backend/dx12_backend.h"

using namespace graphics_sandbox;
using namespace graphics_sandbox::d3d12;

GSWindow gs_create_window(const char* window_name, uint32_t width, uint32_t height, bool fullscreen, uint64_t data[5])
{
    TGraphicSettings settings;
    settings.window_name = window_name;
    settings.width = width;
    settings.height = height;
    settings.fullscreen = fullscreen;
    memcpy(settings.data, data, sizeof(uint64_t) * 5);
    return (GSWindow)window::create_window(settings);
}

void gs_destroy_window(GSWindow renderWindow)
{
    RenderWindow render_window_internal = (RenderWindow)renderWindow;
    window::destroy_window(render_window_internal);
}

void gs_show_window(GSWindow renderWindow)
{
    RenderWindow render_window_internal = (RenderWindow)renderWindow;
    window::show(render_window_internal);
}

void gs_hide_window(GSWindow renderWindow)
{
    RenderWindow render_window_internal = (RenderWindow)renderWindow;
    window::hide(render_window_internal);
}

GSGraphicsDevice gs_create_graphics_device(bool enableDebug, uint32_t preferred_adapter, bool stable_power_state)
{
    return (GSGraphicsDevice)graphics_device::create_graphics_device(enableDebug, preferred_adapter, stable_power_state);
}

void gs_destroy_graphics_device(GSGraphicsDevice graphics_device)
{
    GraphicsDevice graphics_device_internal = (GraphicsDevice)graphics_device;
    graphics_device::destroy_graphics_device(graphics_device_internal);
}

GSCommandQueue gs_create_command_queue(GSGraphicsDevice graphicsDevice)
{
    return (GSCommandQueue)command_queue::create_command_queue((GraphicsDevice)graphicsDevice);
}

void gs_destroy_command_queue(GSCommandQueue commandQueue)
{
    CommandQueue cmdq_internal = (CommandQueue)commandQueue;
    command_queue::destroy_command_queue(cmdq_internal);
}

void gs_execute_command_buffer(GSCommandQueue commandQueue, GSCommandBuffer commandBuffer)
{
    CommandQueue cmdq_internal = (CommandQueue)commandQueue;
    CommandBuffer cmdb_internal = (CommandQueue)commandBuffer;
    command_queue::execute_command_buffer(cmdq_internal, cmdb_internal);
}

void gs_flush_command_queue(GSCommandQueue commandQueue)
{
    CommandQueue cmdq_internal = (CommandQueue)commandQueue;
    command_queue::flush(cmdq_internal);
}
