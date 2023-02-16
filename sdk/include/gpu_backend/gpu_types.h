#pragma once

// External includes
#include <stdint.h>

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

	// Types definition
	typedef uint64_t RenderEnvironment;
	typedef uint64_t RenderWindow;
	typedef uint64_t CommandBuffer;
	typedef uint64_t Fence;
	typedef uint64_t FenceEvent;
}