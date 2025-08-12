#pragma once

#include "render/indirectworkload.h"
#include "render/buffer.h"
#include "render/commandlist.h"
#include "render/gpuresource.h"
#include "render/pipelineobject.h"
#include "render/rootsignature.h"
#include "render/parameterset.h"
#include "core/framework.h"
#include "misc/assert.h"
#include "shaders/ivycommon.h"
#include "shadercompiler.h"
#include <dxcapi.h>

struct IvyRenderIndirect
{
    // Simple draw args for single draw command
    struct DrawIndexedArgs
    {
        uint32_t IndexCountPerInstance;
        uint32_t InstanceCount;
        uint32_t StartIndexLocation;
        int32_t  BaseVertexLocation;
        uint32_t StartInstanceLocation;
    };

    cauldron::Buffer*           m_pArgsBuffer       = nullptr;
    cauldron::IndirectWorkload* m_pIndirectWorkload = nullptr;
    cauldron::RootSignature*    m_pRootSignature    = nullptr;
    cauldron::PipelineObject*   m_pPipelineObject   = nullptr;
    bool                        m_argsReady         = false;

    ~IvyRenderIndirect()
    {
        if (m_pArgsBuffer)
            delete m_pArgsBuffer;
        if (m_pIndirectWorkload)
            delete m_pIndirectWorkload;
        if (m_pPipelineObject)
            delete m_pPipelineObject;
        // Note: m_pRootSignature is shared, don't delete it
    }

    void Init(cauldron::RootSignature* pSharedRootSignature, 
              const cauldron::Texture* pAlbedoRT,
              const cauldron::Texture* pNormalRT,
              const cauldron::Texture* pAoRoughnessMetallicRT,
              const cauldron::Texture* pMotionRT,
              const cauldron::Texture* pDepthRT)
    {
        m_pIndirectWorkload = cauldron::IndirectWorkload::CreateIndirectWorkload(cauldron::IndirectCommandType::DrawIndexed);

        // Create argument buffer for both leaf and stem (two draw commands)
        DrawIndexedArgs dummyArgs[2] = {};

        cauldron::BufferDesc argsDesc = cauldron::BufferDesc::Data(L"Ivy_IndirectArgs", sizeof(DrawIndexedArgs) * 2, sizeof(uint32_t));
        m_pArgsBuffer = cauldron::Buffer::CreateBufferResource(&argsDesc, cauldron::ResourceState::CopyDest);
        m_pArgsBuffer->CopyData(dummyArgs, sizeof(dummyArgs));

        // Use shared root signature from ivyrendermodule
        m_pRootSignature = pSharedRootSignature;

        // Create Pipeline State Object
        cauldron::PipelineDesc psoDesc;
        psoDesc.SetRootSignature(m_pRootSignature);

        // Add vertex shader
        cauldron::ShaderBuildDesc vsDesc = cauldron::ShaderBuildDesc::Vertex(L"ivyleaf_indirect.hlsl", L"VSMain", cauldron::ShaderModel::SM6_0, nullptr);
        psoDesc.AddShaderDesc(vsDesc);

        // Add pixel shader
        cauldron::ShaderBuildDesc psDesc = cauldron::ShaderBuildDesc::Pixel(L"ivyleaf_indirect.hlsl", L"PSMain", cauldron::ShaderModel::SM6_0, nullptr);
        psoDesc.AddShaderDesc(psDesc);

        // Setup input layout for position and normal
        std::vector<cauldron::InputLayoutDesc> vertexInputLayout = {
            cauldron::InputLayoutDesc(cauldron::VertexAttributeType::Position, cauldron::ResourceFormat::RGB32_FLOAT, 0, 0),
            cauldron::InputLayoutDesc(cauldron::VertexAttributeType::Normal, cauldron::ResourceFormat::RGB32_FLOAT, 1, 0)
        };
        psoDesc.AddInputLayout(vertexInputLayout);

        // Setup rasterizer state - disable culling for debugging
        cauldron::RasterDesc rasterDesc;
        rasterDesc.CullingMode = cauldron::CullMode::None;  // Disable culling to see both sides
        rasterDesc.FrontCounterClockwise = true;
        psoDesc.AddRasterStateDescription(&rasterDesc);

        // Setup primitive topology - use triangle list
        psoDesc.AddPrimitiveTopology(cauldron::PrimitiveTopologyType::Triangle);

        // Setup render target formats to match GBuffer
        cauldron::ResourceFormat colorFormats[4] = {
            pAlbedoRT->GetFormat(),
            pNormalRT->GetFormat(),
            pAoRoughnessMetallicRT->GetFormat(),
            pMotionRT->GetFormat()
        };
        psoDesc.AddRenderTargetFormats(4, colorFormats, pDepthRT->GetFormat());

        m_pPipelineObject = cauldron::PipelineObject::CreatePipelineObject(L"IvyIndirect_PSO", psoDesc);

        cauldron::CauldronWarning(L"[IvyRenderIndirect] Init() completed: PSO and dummy args created");
    }

    void Render(cauldron::ParameterSet* pWorkGraphParameterSet,
                const std::vector<const cauldron::Buffer*>* pVertexBuffers = nullptr, 
                const std::vector<const cauldron::Buffer*>* pIndexBuffers = nullptr,
                int ivyLeafSurfaceIndex = -1,
                int ivyStemSurfaceIndex = -1,
                const std::vector<Surface_Info>* pSurfaceBuffer = nullptr)
    {
        static bool sLoggedOnce = false;
        if (!sLoggedOnce)
        {
            cauldron::CauldronWarning(L"[IvyRenderIndirect] Render() entered (ExecuteIndirect no-op)");
            sLoggedOnce = true;
        }

        if (!m_pIndirectWorkload || !m_pArgsBuffer || !m_pPipelineObject)
            return;

        cauldron::CommandList* pCmdList = cauldron::GetFramework()->GetActiveCommandList();
        if (!pCmdList)
            return;

        // Set pipeline state
        cauldron::SetPipelineState(pCmdList, m_pPipelineObject);

        // Set primitive topology to triangle list
        cauldron::SetPrimitiveTopology(pCmdList, cauldron::PrimitiveTopology::TriangleList);

        // Bind the work graph parameter set to get access to constant buffer (ViewProjection matrix)
        if (pWorkGraphParameterSet)
        {
            pWorkGraphParameterSet->Bind(pCmdList, m_pPipelineObject);
        }

        // Prepare draw arguments for leaf and stem
        DrawIndexedArgs drawCommands[2] = {};
        int validDrawCommands = 0;

        // Set up vertex and index buffers and prepare draw commands
        if (pVertexBuffers && pIndexBuffers && pSurfaceBuffer)
        {
            // Handle leaf geometry (index 0)
            if (ivyLeafSurfaceIndex >= 0 && ivyLeafSurfaceIndex < static_cast<int>(pSurfaceBuffer->size()))
            {
                const Surface_Info& leafSurfaceInfo = (*pSurfaceBuffer)[ivyLeafSurfaceIndex];
                
                if (leafSurfaceInfo.position_attribute_offset >= 0 && 
                    leafSurfaceInfo.position_attribute_offset < static_cast<int>(pVertexBuffers->size()) &&
                    leafSurfaceInfo.normal_attribute_offset >= 0 && 
                    leafSurfaceInfo.normal_attribute_offset < static_cast<int>(pVertexBuffers->size()) &&
                    leafSurfaceInfo.index_offset >= 0 && 
                    leafSurfaceInfo.index_offset < static_cast<int>(pIndexBuffers->size()))
                {
                    drawCommands[0].IndexCountPerInstance = leafSurfaceInfo.num_indices;
                    drawCommands[0].InstanceCount = 1;
                    drawCommands[0].StartIndexLocation = 0;
                    drawCommands[0].BaseVertexLocation = 0;
                    drawCommands[0].StartInstanceLocation = 0;
                    validDrawCommands++;

                    cauldron::CauldronWarning(L"[IvyRenderIndirect] Leaf geometry prepared: indices=%d", leafSurfaceInfo.num_indices);
                }
            }

            // Handle stem geometry (index 1)
            if (ivyStemSurfaceIndex >= 0 && ivyStemSurfaceIndex < static_cast<int>(pSurfaceBuffer->size()))
            {
                const Surface_Info& stemSurfaceInfo = (*pSurfaceBuffer)[ivyStemSurfaceIndex];
                
                if (stemSurfaceInfo.position_attribute_offset >= 0 && 
                    stemSurfaceInfo.position_attribute_offset < static_cast<int>(pVertexBuffers->size()) &&
                    stemSurfaceInfo.normal_attribute_offset >= 0 && 
                    stemSurfaceInfo.normal_attribute_offset < static_cast<int>(pVertexBuffers->size()) &&
                    stemSurfaceInfo.index_offset >= 0 && 
                    stemSurfaceInfo.index_offset < static_cast<int>(pIndexBuffers->size()))
                {
                    drawCommands[1].IndexCountPerInstance = stemSurfaceInfo.num_indices;
                    drawCommands[1].InstanceCount = 1;
                    drawCommands[1].StartIndexLocation = 0;
                    drawCommands[1].BaseVertexLocation = 0;
                    drawCommands[1].StartInstanceLocation = 0;
                    validDrawCommands = 2; // We have both leaf and stem

                    cauldron::CauldronWarning(L"[IvyRenderIndirect] Stem geometry prepared: indices=%d", stemSurfaceInfo.num_indices);
                }
            }
        }

        // Update argument buffer with both draw commands
        if (validDrawCommands > 0)
        {
            m_pArgsBuffer->CopyData(drawCommands, sizeof(DrawIndexedArgs) * 2);
            cauldron::CauldronWarning(L"[IvyRenderIndirect] Updated argument buffer with %d draw commands", validDrawCommands);
        }

        // Ensure argument buffer is ready for indirect argument usage
        if (!m_argsReady)
        {
            cauldron::Barrier barrier = cauldron::Barrier::Transition(
                m_pArgsBuffer->GetResource(), cauldron::ResourceState::CopyDest, cauldron::ResourceState::IndirectArgument);
            cauldron::ResourceBarrier(pCmdList, 1, &barrier);
            m_argsReady = true;
        }

        // Execute leaf geometry if valid
        if (validDrawCommands >= 1 && ivyLeafSurfaceIndex >= 0 && pVertexBuffers && pIndexBuffers && pSurfaceBuffer)
        {
            const Surface_Info& leafSurfaceInfo = (*pSurfaceBuffer)[ivyLeafSurfaceIndex];
            
            // Set vertex and index buffers for leaf geometry
            cauldron::BufferAddressInfo positionBufferInfo = (*pVertexBuffers)[leafSurfaceInfo.position_attribute_offset]->GetAddressInfo();
            cauldron::BufferAddressInfo normalBufferInfo = (*pVertexBuffers)[leafSurfaceInfo.normal_attribute_offset]->GetAddressInfo();
            
            cauldron::BufferAddressInfo vertexBuffers[2] = { positionBufferInfo, normalBufferInfo };
            cauldron::SetVertexBuffers(pCmdList, 0, 2, vertexBuffers);

            cauldron::BufferAddressInfo indexBufferInfo = (*pIndexBuffers)[leafSurfaceInfo.index_offset]->GetAddressInfo();
            cauldron::SetIndexBuffer(pCmdList, &indexBufferInfo);

            // Execute first draw command (leaf) using offset 0
            cauldron::ExecuteIndirect(pCmdList, m_pIndirectWorkload, m_pArgsBuffer, 1 /*drawCount*/, 0 /*offset*/);
            cauldron::CauldronWarning(L"[IvyRenderIndirect] Executed leaf geometry");
        }

        // Execute stem geometry if valid
        if (validDrawCommands >= 2 && ivyStemSurfaceIndex >= 0 && pVertexBuffers && pIndexBuffers && pSurfaceBuffer)
        {
            const Surface_Info& stemSurfaceInfo = (*pSurfaceBuffer)[ivyStemSurfaceIndex];
            
            // Set vertex and index buffers for stem geometry
            cauldron::BufferAddressInfo positionBufferInfo = (*pVertexBuffers)[stemSurfaceInfo.position_attribute_offset]->GetAddressInfo();
            cauldron::BufferAddressInfo normalBufferInfo = (*pVertexBuffers)[stemSurfaceInfo.normal_attribute_offset]->GetAddressInfo();
            
            cauldron::BufferAddressInfo vertexBuffers[2] = { positionBufferInfo, normalBufferInfo };
            cauldron::SetVertexBuffers(pCmdList, 0, 2, vertexBuffers);

            cauldron::BufferAddressInfo indexBufferInfo = (*pIndexBuffers)[stemSurfaceInfo.index_offset]->GetAddressInfo();
            cauldron::SetIndexBuffer(pCmdList, &indexBufferInfo);

            // Execute second draw command (stem) using offset sizeof(DrawIndexedArgs)
            cauldron::ExecuteIndirect(pCmdList, m_pIndirectWorkload, m_pArgsBuffer, 1 /*drawCount*/, sizeof(DrawIndexedArgs) /*offset*/);
            cauldron::CauldronWarning(L"[IvyRenderIndirect] Executed stem geometry");
        }
    }
};
