#pragma once

// Bento includes
#include <bento_collection/dynamic_string.h>

namespace graphics_sandbox
{
	struct ComputeShaderDescriptor
	{
		ALLOCATOR_BASED;
		ComputeShaderDescriptor(bento::IAllocator& allocator);

		// Internal data
		bento::DynamicString filename;
		bento::DynamicString kernelname;
		uint32_t uavCount;
		uint32_t srvCount;
		uint32_t cbvCount;
		bento::IAllocator& _allocator;
	};
}