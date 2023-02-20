#pragma once

// Bento includes
#include <bento_base/platform.h>

namespace graphics_sandbox
{
	// Various graphics formats supported by the sandbox
	enum class GraphicsFormat
	{
		// R8G8B8A8 Formats
		R8G8B8A8_SNorm,
		R8G8B8A8_UNorm,
		R8G8B8A8_UInt,
		R8G8B8A8_SInt,

		// R16G16B16A16 Formats
		R16G16B16A16_SFloat,
		R16G16B16A16_UInt,
		R16G16B16A16_SInt,

		// Depth buffer formats
		Depth32,
		Depth24Stencil8,

		Count
	};

	// Function that returns if a function is a depth-compatible format
	bool is_depth_format(GraphicsFormat graphicsFormat);

	// Function that returns the format alignement
	uint8_t graphics_format_alignement(GraphicsFormat graphicsFormat);
}