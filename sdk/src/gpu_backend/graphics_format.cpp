// SDK Inlcudes
#include "gpu_backend/graphics_format.h"

namespace graphics_sandbox
{
	bool is_depth_format(GraphicsFormat graphicsFormat)
	{
		return graphicsFormat == GraphicsFormat::Depth32 || graphicsFormat == GraphicsFormat::Depth24Stencil8;
	}

	uint8_t graphics_format_alignement(GraphicsFormat graphicsFormat)
	{
		switch (graphicsFormat)
		{
			// R8G8B8A8 Formats
			case GraphicsFormat::R8G8B8A8_SNorm:
			case GraphicsFormat::R8G8B8A8_UNorm:
			case GraphicsFormat::R8G8B8A8_UInt:
			case GraphicsFormat::R8G8B8A8_SInt:
				return 4;

			// R16G16B16A16 Formats
			case GraphicsFormat::R16G16B16A16_SFloat:
			case GraphicsFormat::R16G16B16A16_UInt:
			case GraphicsFormat::R16G16B16A16_SInt:
				return 8;

			// Depth/Stencil formats
			case GraphicsFormat::Depth32:
			case GraphicsFormat::Depth24Stencil8:
				return 4;
		}

		// SHould never get here
		return 4;
	}
}