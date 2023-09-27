#pragma once

// External includes
#include <stdint.h>

#ifdef WINDOWSPC
	#define GS_EXPORT __declspec(dllexport)
#else
	#define GS_EXPORT
#endif

// Set of required types
typedef uint64_t GSAllocator;
typedef uint64_t GSGraphicsDevice;
typedef uint64_t GSWindow;
typedef uint64_t GSCommandQueue;
typedef uint64_t GSCommandBuffer;
