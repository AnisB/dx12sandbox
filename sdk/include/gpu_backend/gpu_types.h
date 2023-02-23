#pragma once

// External includes
#include <stdint.h>

namespace graphics_sandbox
{
	// General structures
	typedef uint64_t RenderWindow;
	typedef uint64_t GraphicsDevice;
	typedef uint64_t CommandQueue;
	typedef uint64_t SwapChain;

	// Command buffer
	typedef uint64_t CommandBuffer;

	// Syncronization
	typedef uint64_t Fence;
	typedef uint64_t FenceEvent;

	// Shaders
	typedef uint64_t ComputeShader;

	// Resources
	typedef uint64_t Resource;
	typedef uint64_t DescriptorHeap;
	typedef uint64_t RenderTexture;
	typedef uint64_t GraphicsBuffer;
	typedef uint64_t ConstantBuffer;
}