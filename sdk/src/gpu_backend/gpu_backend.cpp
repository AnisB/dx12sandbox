// Bento includes
#include <bento_base/security.h>

// Library includes
#include "gpu_backend/gpu_backend.h"

namespace graphics_sandbox
{
	// Variable that holds the gpu backend
	GPUBackendAPI gpuBackendAPI[RenderingBackEnd::COUNT] = { };

	void initialize_gpu_backend(RenderingBackEnd::Type backend_type)
	{
		// Grab the target api to 
		GPUBackendAPI& currentGPUAPI = gpuBackendAPI[backend_type];
	}

	const GPUBackendAPI* gpu_api(RenderingBackEnd::Type backend_type)
	{
		return &gpuBackendAPI[backend_type];
	}
}