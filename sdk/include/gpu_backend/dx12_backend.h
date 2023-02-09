#pragma once
#ifdef D3D12_SUPPORTED

// Library includes
#include "gpu_backend.h"

namespace graphics_sandbox
{
	namespace d3d12
	{
		TGraphicSettings default_settings();

		namespace render_system
		{
			// Global initialize of the graphics system
			bool init_render_system();
			void shutdown_render_system();

			// Render environment functions
			RenderEnvironment create_render_environment(const TGraphicSettings& graphic_settings);
			RenderWindow render_window(RenderEnvironment render_environement);
			void destroy_render_environment(RenderEnvironment render_environment);
		}

		namespace window
		{
			void show(RenderWindow window);
			void hide(RenderWindow window);
		}

		namespace command_buffer
		{
			CommandBuffer create_command_buffer(RenderEnvironment render_environement);
			void destroy_command_buffer(CommandBuffer command_buffer);
		}

		namespace graphics_fence
		{
			Fence create_fence(RenderEnvironment render_environment);
			void destroy_fence(Fence fence);
		}
	}
}
#endif