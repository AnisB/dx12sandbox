// SDK includes
#include "gpu_backend/compute_shader_descriptor.h"

namespace graphics_sandbox
{
	ComputeShaderDescriptor::ComputeShaderDescriptor(bento::IAllocator& allocator)
	: _allocator(allocator)
	, filename(allocator)
	, kernelname(allocator)
	{
	}
}