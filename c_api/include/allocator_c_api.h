#pragma once

#include "types_c_api.h"

extern "C"
{
	// Function to create a new allocator
	GS_EXPORT GSAllocator gs_create_allocator();

	// Function to destroy a noiseless allocator
	GS_EXPORT void gs_destroy_allocator(GSAllocator allocator);
}