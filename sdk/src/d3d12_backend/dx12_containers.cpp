// SDK includs
#include "d3d12_backend/dx12_containers.h"

// Bento includes
#include <bento_base/security.h>

namespace graphics_sandbox
{
	namespace d3d12
	{
		DXGI_FORMAT graphics_format_to_dxgi_format(GraphicsFormat graphicsFormat)
		{
			switch (graphicsFormat)
			{
				// R8G8B8A8 Formats
			case GraphicsFormat::R8G8B8A8_SNorm:
				return DXGI_FORMAT_R8G8B8A8_SNORM;
			case GraphicsFormat::R8G8B8A8_UNorm:
				return DXGI_FORMAT_R8G8B8A8_UNORM;
			case GraphicsFormat::R8G8B8A8_UInt:
				return DXGI_FORMAT_R8G8B8A8_UINT;
			case GraphicsFormat::R8G8B8A8_SInt:
				return DXGI_FORMAT_R8G8B8A8_SINT;

				// R16G16B16A16 Formats
			case GraphicsFormat::R16G16B16A16_SFloat:
				return DXGI_FORMAT_R16G16B16A16_FLOAT;
			case GraphicsFormat::R16G16B16A16_UInt:
				return DXGI_FORMAT_R16G16B16A16_UINT;
			case GraphicsFormat::R16G16B16A16_SInt:
				return DXGI_FORMAT_R16G16B16A16_SINT;

				// Depth/Stencil formats
			case GraphicsFormat::Depth32:
				return DXGI_FORMAT_D32_FLOAT;
			case GraphicsFormat::Depth24Stencil8:
				return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
			}

			// Should never be here
			assert_fail_msg("Unknown DX12 Format");
			return DXGI_FORMAT_R8G8B8A8_SNORM;
		}

		D3D12_RESOURCE_DIMENSION texture_dimension_to_dx12_resource_dimension(TextureDimension textureDimension)
		{
			switch (textureDimension)
			{
			case TextureDimension::Tex1D:
			case TextureDimension::Tex1DArray:
				return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
			case TextureDimension::Tex2D:
			case TextureDimension::TexCube:
				return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			case TextureDimension::Tex3D:
			case TextureDimension::TexCubeArray:
			case TextureDimension::Tex2DArray:
				return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
			default:
				return D3D12_RESOURCE_DIMENSION_UNKNOWN;
			}
		}
	}
}