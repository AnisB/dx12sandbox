#pragma once

// Internal include
#include "gpu_types.h"

// External includes
#include <stdint.h>
#include <string>
	
namespace graphics_sandbox
{
	namespace RenderingBackEnd
	{
		enum Type
		{
			DX12 = 0,
			COUNT = 1
		};
	}

	struct TGraphicSettings
	{
		std::string window_name;
		uint32_t width;
		uint32_t height;
		bool fullscreen;
		RenderingBackEnd::Type backend;
		uint64_t data[5];
	};
}
