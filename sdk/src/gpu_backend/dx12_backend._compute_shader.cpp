// Bento includes
#include <bento_base/security.h>
#include <bento_memory/common.h>
#include <bento_base/log.h>

// Internal includes
#include "gpu_backend/dx12_backend.h"
#include "gpu_backend/dx12_containers.h"
#include "gpu_backend/dx12_utilities.h"

namespace graphics_sandbox
{
	namespace d3d12
	{
		namespace compute_shader
		{
			ComputeShader create_compute_shader(GraphicsDevice graphicsDevice, const ComputeShaderDescriptor& csd)
			{
				// Convert the strings to wide
				const std::wstring& filename = convert_to_wide(csd.filename.c_str(), csd.filename.size());
				const std::wstring& kernelName = convert_to_wide(csd.kernelname.c_str(), csd.kernelname.size());

				// Create the library and the compiler
				IDxcLibrary* library;
				DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));

				// Load the file into a blob
				uint32_t code_page = CP_UTF8;
				IDxcBlobEncoding* source_blob;
				assert_msg(library->CreateBlobFromFile(filename.c_str(), &code_page, &source_blob) == S_OK, "Failed to load the shader code.");

				// Release the library
				library->Release();

				// Compilation arguments
				LPCWSTR arguments[] = { L"-O3"};

				// Create the compiler
				IDxcCompiler* compiler;
				DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));

				// Compile the shader
				IDxcOperationResult* result;
				HRESULT hr = compiler->Compile(source_blob, filename.c_str(), kernelName.c_str(), L"cs_6_4", arguments, _countof(arguments), nullptr, 0, nullptr, &result);
				if (SUCCEEDED(hr))
					result->GetStatus(&hr);
				bool compile_succeed = SUCCEEDED(hr);

				// If the compilation failed, print the error
				IDxcBlobEncoding* error_blob;
				if (SUCCEEDED(result->GetErrorBuffer(&error_blob)) && error_blob)
				{
					// Log the compilation message
					bento::default_logger()->log(bento::LogLevel::info, "Compilation", error_blob->GetBufferSize() != 0 ? (const char*)error_blob->GetBufferPointer() : "Successfully compiled kernel.");

					// Release the error blob
					error_blob->Release();
				}

				// If succeeded, grab the right pointer
				IDxcBlob* shader_blob = 0;
				if (compile_succeed)
					result->GetResult(&shader_blob);

				// Release all the intermediate resources
				result->Release();
				source_blob->Release();
				compiler->Release();
	
				// Grab the graphics device
				DX12GraphicsDevice* deviceI = (DX12GraphicsDevice*)graphicsDevice;
				assert(deviceI != nullptr);
				ID3D12Device2* device = deviceI->device;

				// Create the root signature for the shader
				D3D12_ROOT_PARAMETER rootParameters[3];
				D3D12_DESCRIPTOR_RANGE descRange[3];

				// Tracking the current count/index
				uint8_t cdIndex = 0;

				// Process the SRVS
				if (csd.srvCount > 0)
				{
					descRange[cdIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
					descRange[cdIndex].NumDescriptors = csd.srvCount;
					descRange[cdIndex].BaseShaderRegister = 0; // t0..tN
					descRange[cdIndex].RegisterSpace = 0;
					descRange[cdIndex].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
					rootParameters[cdIndex].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
					rootParameters[cdIndex].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
					rootParameters[cdIndex].DescriptorTable.NumDescriptorRanges = 1;
					rootParameters[cdIndex].DescriptorTable.pDescriptorRanges = &descRange[cdIndex];
					cdIndex++;
				}

				// Process the UAVs
				if (csd.uavCount > 0)
				{
					descRange[cdIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
					descRange[cdIndex].NumDescriptors = csd.uavCount;
					descRange[cdIndex].BaseShaderRegister = 0; // u0..uN
					descRange[cdIndex].RegisterSpace = 0;
					descRange[cdIndex].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
					rootParameters[cdIndex].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
					rootParameters[cdIndex].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
					rootParameters[cdIndex].DescriptorTable.NumDescriptorRanges = 1;
					rootParameters[cdIndex].DescriptorTable.pDescriptorRanges = &descRange[cdIndex];
					cdIndex++;
				}

				// Process the CBVs
				if (csd.cbvCount > 0)
				{
					descRange[cdIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
					descRange[cdIndex].NumDescriptors = csd.cbvCount;
					descRange[cdIndex].BaseShaderRegister = 0; // b0..bN
					descRange[cdIndex].RegisterSpace = 0;
					descRange[cdIndex].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
					rootParameters[cdIndex].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
					rootParameters[cdIndex].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
					rootParameters[cdIndex].DescriptorTable.NumDescriptorRanges = 1;
					rootParameters[cdIndex].DescriptorTable.pDescriptorRanges = &descRange[cdIndex];
					cdIndex++;
				}

				D3D12_ROOT_SIGNATURE_DESC desc = {};
				desc.NumParameters = cdIndex;
				desc.pParameters = rootParameters;
				desc.NumStaticSamplers = 0;
				desc.pStaticSamplers = nullptr;

				// Create the signature blob
				ID3DBlob* signatureBlob;
				assert_msg(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, nullptr) == S_OK, "Failed to create root singnature blob.");

				// Create the root signature
				ID3D12RootSignature* rootSignature;
				assert_msg(device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)) == S_OK, "Failed to create root signature.");

				// Release the resources
				signatureBlob->Release();

				// Create the descriptor heap for this compute shader
				ID3D12DescriptorHeap* descHeap = (ID3D12DescriptorHeap*)descriptor_heap::create_descriptor_heap(graphicsDevice, csd.srvCount + csd.uavCount + csd.cbvCount, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				
				// Create the pipeline state object for the shader
				D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.pRootSignature = rootSignature;
				pso_desc.CS.BytecodeLength = shader_blob->GetBufferSize();
				pso_desc.CS.pShaderBytecode = shader_blob->GetBufferPointer();
				ID3D12PipelineState* pso;
				assert_msg(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pso)) == S_OK, "Failed to create pipeline state object.");

				// Create the compute shader structure
				DX12ComputeShader* cS = bento::make_new<DX12ComputeShader>(*bento::common_allocator());
				cS->shaderBlob = shader_blob;
				cS->rootSignature = rootSignature;
				cS->pipelineStateObject = pso;
				cS->descriptorHeap = descHeap;

				// Number of resources per type
				cS->srvCount = csd.srvCount;
				cS->uavCount = csd.uavCount;
				cS->cbvCount = csd.cbvCount;

				// Pre-evaluate the CPU Heap handles
				uint32_t descSize = deviceI->descriptorSize[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
				cS->srvCPU = descHeap->GetCPUDescriptorHandleForHeapStart();
				cS->uavCPU = cS->srvCPU;
				cS->uavCPU.ptr += (uint64_t)csd.srvCount * descSize;
				cS->cbvCPU = cS->uavCPU;
				cS->cbvCPU.ptr += (uint64_t)csd.uavCount * descSize;

				// Pre-evaluate the GPU Heap handles
				cS->srvGPU = descHeap->GetGPUDescriptorHandleForHeapStart();
				cS->uavGPU = cS->srvGPU;
				cS->uavGPU.ptr += (uint64_t)csd.srvCount * descSize;
				cS->cbvGPU = cS->uavGPU;
				cS->cbvGPU.ptr += (uint64_t)csd.uavCount * descSize;

				// Convert to the opaque structure
				return (ComputeShader)cS;
			}

			void destroy_compute_shader(ComputeShader computeShader)
			{
				DX12ComputeShader* dx12_computeShader = (DX12ComputeShader*)computeShader;
				dx12_computeShader->descriptorHeap->Release();
				dx12_computeShader->rootSignature->Release();
				dx12_computeShader->shaderBlob->Release();
				bento::make_delete<DX12ComputeShader>(*bento::common_allocator(), dx12_computeShader);
			}
		}
	}
}
