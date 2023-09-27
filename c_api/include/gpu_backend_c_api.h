#pragma once

#include "types_c_api.h"

extern "C"
{
	// Window
	GS_EXPORT GSWindow gs_create_window(const char* window_name, uint32_t width, uint32_t height, bool fullscreen, uint64_t data[5]);
	GS_EXPORT void gs_destroy_window(GSWindow renderWindow);
	GS_EXPORT void gs_show_window(GSWindow renderWindow);
	GS_EXPORT void gs_hide_window(GSWindow renderWindow);

	// Graphics device
	GS_EXPORT GSGraphicsDevice gs_create_graphics_device(bool enableDebug = false, uint32_t preferred_adapter = UINT32_MAX, bool stable_power_state = false);
	GS_EXPORT void gs_destroy_graphics_device(GSGraphicsDevice graphics_device);

	// Command queue
	GS_EXPORT GSCommandQueue gs_create_command_queue(GSGraphicsDevice graphicsDevice);
	GS_EXPORT void gs_destroy_command_queue(GSCommandQueue commandQueue);
	GS_EXPORT void gs_execute_command_buffer(GSCommandQueue commandQueue, GSCommandBuffer commandBuffer);
	GS_EXPORT void gs_flush_command_queue(GSCommandQueue commandQueue);
}