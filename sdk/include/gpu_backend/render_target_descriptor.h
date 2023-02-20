#pragma once

// Bento includs
#include <bento_math/types.h>

// SDK includs
#include "gpu_backend/graphics_format.h"

namespace graphics_sandbox
{
	enum class TextureDimension
	{
		Tex1D,
		Tex1DArray,
		Tex2D,
		Tex2DArray,
		Tex3D,
		TexCube,
		TexCubeArray
	};

	struct RenderTextureDescriptor
	{
		TextureDimension dimension;
		uint32_t width;
		uint32_t height;
		uint32_t depth;
		bool hasMips;
		bool isUAV;
		GraphicsFormat format;
		bento::Vector4 clearColor;
	};
}