#pragma once

// Internal include
#include "gpu_types.h"

// External includes
#include <stdint.h>
#include <string>
	
namespace graphics_sandbox
{
	enum class RenderingBackEnd
	{
		DX12 = 0,
		COUNT = 1
	};

	struct TGraphicSettings
	{
		std::string window_name;
		uint32_t width;
		uint32_t height;
		bool fullscreen;
		RenderingBackEnd backend;
		uint64_t data[5];

		TGraphicSettings()
		{
			window_name = "Graphics Sandbox";
			width = 1280;
			height = 720;
			fullscreen = false;
			backend = RenderingBackEnd::DX12;
			memset(data, 0, sizeof(data));
		}
	};
}
