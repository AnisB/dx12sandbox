#pragma once

// Library includes
#include "gpu_backend/settings.h"

// Bento includes
#include "bento_math/types.h"

// External includes
#include <stdint.h>

namespace graphics_sandbox
{
	// Foward declare 
	struct TMaterial;

	struct GPURenderSystemAPI
	{
	};

	struct GPUBackendAPI
	{
		GPURenderSystemAPI render_system_api;
		TGraphicSettings default_settings;
	};

	// Initialize the target api
	void initialize_gpu_backend(RenderingBackEnd::Type backend_type);

	// Request the target gpu API
	const GPUBackendAPI* gpu_api(RenderingBackEnd::Type backend_type);
}