// Bento includes
#include <bento_base/security.h>
#include <bento_memory/common.h>
#include <bento_base/log.h>

// Internal includes
#include "d3d12_backend/dx12_backend.h"
#include "d3d12_backend/dx12_containers.h"
#include "tools/string_utilities.h"

namespace graphics_sandbox
{
    namespace d3d12
    {
        namespace compute_shader
        {
            DX12DescriptorHeap create_desriptor_heap(GraphicsDevice graphicsDevice, uint32_t srvCount, uint32_t uavCount, uint32_t cbvCount)
            {
                DX12GraphicsDevice* deviceI = (DX12GraphicsDevice*)graphicsDevice;

                // Create the descriptor heap for this compute shader
                DX12DescriptorHeap descriptorHeap;
                ID3D12DescriptorHeap* descHeap = (ID3D12DescriptorHeap*)descriptor_heap::create_descriptor_heap(graphicsDevice, srvCount + uavCount + cbvCount, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                descriptorHeap.descriptorHeap = descHeap;

                // Pre-evaluate the CPU Heap handles
                uint32_t descSize = deviceI->descriptorSize[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
                descriptorHeap.srvCPU = descHeap->GetCPUDescriptorHandleForHeapStart();
                descriptorHeap.uavCPU = descriptorHeap.srvCPU;
                descriptorHeap.uavCPU.ptr += (uint64_t)srvCount * descSize;
                descriptorHeap.cbvCPU = descriptorHeap.uavCPU;
                descriptorHeap.cbvCPU.ptr += (uint64_t)uavCount * descSize;

                // Pre-evaluate the GPU Heap handles
                descriptorHeap.srvGPU = descHeap->GetGPUDescriptorHandleForHeapStart();
                descriptorHeap.uavGPU = descriptorHeap.srvGPU;
                descriptorHeap.uavGPU.ptr += (uint64_t)srvCount * descSize;
                descriptorHeap.cbvGPU = descriptorHeap.uavGPU;
                descriptorHeap.cbvGPU.ptr += (uint64_t)uavCount * descSize;

                return descriptorHeap;
            }

            void destroy_descriptor_heap(DX12DescriptorHeap& descriptorHeap)
            {
                descriptorHeap.descriptorHeap->Release();
            }

            void validate_compute_shader_heap(DX12ComputeShader* computeShader, uint32_t cmdBatchIndex)
            {
                // We need to check if we've entered a new frame. If it is the case we just need to:
                //  - Update the next usable heap to the first one and we're done
                //  - Update the command buffer batch index accordinly
                // This is true because we always have at least one allocated heap.
                if (computeShader->cmdBatchIndex != cmdBatchIndex)
                {
                    computeShader->nextUsableHeap = 0;
                    computeShader->cmdBatchIndex = cmdBatchIndex;
                    return;
                }

                // If we're tracking properly the current batch but a new heap is required, we need to allocate it.
                if (computeShader->descriptorHeaps.size() == computeShader->nextUsableHeap)
                    computeShader->descriptorHeaps.push_back(create_desriptor_heap((GraphicsDevice)computeShader->device, computeShader->srvCount, computeShader->uavCount, computeShader->cbvCount));
            }

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
                bento::Vector<std::wstring> includeDirs(*bento::common_allocator());
                for (uint32_t includeDirIdx = 0; includeDirIdx < csd.includeDirectories.size(); ++includeDirIdx)
                {
                    std::wstring includeArg = L"-I ";
                    auto& includeDir = csd.includeDirectories[includeDirIdx];
                    includeArg += convert_to_wide(includeDir.c_str(), includeDir.size());
                    includeDirs.push_back(includeArg.c_str());
                }

                bento::Vector<LPCWSTR> arguments(*bento::common_allocator());
                arguments.push_back(L"-O3");
                for (uint32_t includeDirIdx = 0; includeDirIdx < csd.includeDirectories.size(); ++includeDirIdx)
                    arguments.push_back(includeDirs[includeDirIdx].c_str());

                // Create the compiler
                IDxcCompiler* compiler;
                DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));

                // Compile the shader
                IDxcOperationResult* result;
                HRESULT hr = compiler->Compile(source_blob, filename.c_str(), kernelName.c_str(), L"cs_6_4", arguments.begin(), arguments.size(), nullptr, 0, nullptr, &result);
                if (SUCCEEDED(hr))
                    result->GetStatus(&hr);
                bool compile_succeed = SUCCEEDED(hr);

                // If the compilation failed, print the error
                IDxcBlobEncoding* error_blob;
                if (SUCCEEDED(result->GetErrorBuffer(&error_blob)) && error_blob)
                {
                    // Log the compilation message
                    bento::default_logger()->log(bento::LogLevel::info, "Compilation", csd.kernelname.c_str());
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
                ID3D12Device2* device = deviceI->device;

                // Create the root signature for the shader
                D3D12_ROOT_PARAMETER rootParameters[3];
                D3D12_DESCRIPTOR_RANGE descRange[3];

                // Create our internal structure
                DX12ComputeShader* cS = bento::make_new<DX12ComputeShader>(*bento::common_allocator(), *bento::common_allocator());
                cS->srvIndex = UINT32_MAX;
                cS->uavIndex = UINT32_MAX;
                cS->cbvIndex = UINT32_MAX;

                // Tracking the current count/index
                uint8_t cdIndex = 0;

                // Process the SRVS
                if (csd.srvCount > 0)
                {
                    cS->srvIndex = cdIndex;
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
                    cS->uavIndex = cdIndex;
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
                    cS->cbvIndex = cdIndex;
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
                    
                // Create the pipeline state object for the shader
                D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
                pso_desc.pRootSignature = rootSignature;
                pso_desc.CS.BytecodeLength = shader_blob->GetBufferSize();
                pso_desc.CS.pShaderBytecode = shader_blob->GetBufferPointer();
                ID3D12PipelineState* pso;
                assert_msg(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pso)) == S_OK, "Failed to create pipeline state object.");

                // Fill the compute shader structure
                cS->device = deviceI;
                cS->shaderBlob = shader_blob;
                cS->rootSignature = rootSignature;
                cS->pipelineStateObject = pso;
                cS->srvCount = csd.srvCount;
                cS->uavCount = csd.uavCount;
                cS->cbvCount = csd.cbvCount;

                // Create the descriptor heap for this compute shader
                cS->descriptorHeaps.push_back(create_desriptor_heap(graphicsDevice, csd.srvCount, csd.uavCount, csd.cbvCount));
                cS->cmdBatchIndex = UINT32_MAX;
                cS->nextUsableHeap = 0;

                // Convert to the opaque structure
                return (ComputeShader)cS;
            }

            void destroy_compute_shader(ComputeShader computeShader)
            {
                // Grab the internal structure
                DX12ComputeShader* dx12_computeShader = (DX12ComputeShader*)computeShader;

                // Destroy all the descriptor heaps
                uint32_t numDescriptorHeaps = (uint32_t)dx12_computeShader->descriptorHeaps.size();
                for (uint32_t heapIdx = 0; heapIdx < numDescriptorHeaps; ++heapIdx)
                    destroy_descriptor_heap(dx12_computeShader->descriptorHeaps[heapIdx]);

                dx12_computeShader->rootSignature->Release();
                dx12_computeShader->shaderBlob->Release();
                
                bento::make_delete<DX12ComputeShader>(*bento::common_allocator(), dx12_computeShader);
            }
        }
    }
}
