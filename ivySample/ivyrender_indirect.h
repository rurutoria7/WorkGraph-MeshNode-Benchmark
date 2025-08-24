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
#include "misc/math.h"
#include "shaders/ivycommon.h"
#include "shadercompiler.h"
#include <dxcapi.h>

struct IvyRenderIndirect
{
    cauldron::IndirectWorkload* m_pIndirectWorkload = nullptr;
    cauldron::RootSignature*    m_pRootSignature    = nullptr;  // Own Graphics Root Signature
    cauldron::ParameterSet*     m_pParameterSet     = nullptr;  // Own ParameterSet
    cauldron::PipelineObject*   m_pPipelineObject   = nullptr;
    
    // Track binding state to avoid redundant SetBufferSRV calls
    bool m_instanceBuffersBound = false;
    

    ~IvyRenderIndirect()
    {
        if (m_pIndirectWorkload)
            delete m_pIndirectWorkload;
        if (m_pParameterSet)
            delete m_pParameterSet;
        if (m_pRootSignature)
            delete m_pRootSignature;
        if (m_pPipelineObject)
            delete m_pPipelineObject;
    }

    void Init(const cauldron::Texture* pAlbedoRT,
              const cauldron::Texture* pNormalRT,
              const cauldron::Texture* pAoRoughnessMetallicRT,
              const cauldron::Texture* pMotionRT,
              const cauldron::Texture* pDepthRT)
    {
        m_pIndirectWorkload = cauldron::IndirectWorkload::CreateIndirectWorkload(cauldron::IndirectCommandType::DrawIndexed);

        // Note: Argument buffer and instance buffers are now created and managed by IvyRenderModule

        // Create independent Graphics Root Signature for ExecuteIndirect
        cauldron::RootSignatureDesc execIndirectRootSigDesc;
        execIndirectRootSigDesc.AddConstantBufferView(0, cauldron::ShaderBindStage::Vertex, 1);        // ViewProjection CBV
        execIndirectRootSigDesc.AddBufferSRVSet(0, cauldron::ShaderBindStage::Vertex, 2);             // Instance buffer SRV Array (t0, t1)
        execIndirectRootSigDesc.AddConstantBufferView(1, cauldron::ShaderBindStage::Vertex, 1);       // instance_buffer_index (b1)
        execIndirectRootSigDesc.m_PipelineType = cauldron::PipelineType::Graphics;
        
        m_pRootSignature = cauldron::RootSignature::CreateRootSignature(L"ExecuteIndirect_RootSignature", execIndirectRootSigDesc);
        
        // Create ParameterSet for this Root Signature
        m_pParameterSet = cauldron::ParameterSet::CreateParameterSet(m_pRootSignature);
        
        // Initialize root constant buffer resources
        m_pParameterSet->SetRootConstantBufferResource(cauldron::GetDynamicBufferPool()->GetResource(), sizeof(Mat4), 0);     // b0: ViewProjection
        m_pParameterSet->SetRootConstantBufferResource(cauldron::GetDynamicBufferPool()->GetResource(), sizeof(uint32_t) * 4, 1); // b1: instance_buffer_index (padded to 16 bytes)

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


    void Render(cauldron::CommandList* pCmdList,  // Pass command list to ensure consistency
                const Mat4& viewProjectionMatrix,
                const cauldron::Buffer* pArgumentBuffer,  // Now passed from IvyRenderModule
                const std::vector<const cauldron::Buffer*>* pVertexBuffers = nullptr, 
                const std::vector<const cauldron::Buffer*>* pIndexBuffers = nullptr,
                int ivyLeafSurfaceIndex = -1,
                int ivyStemSurfaceIndex = -1,
                const std::vector<Surface_Info>* pSurfaceBuffer = nullptr,
                const cauldron::Buffer* pStemInstanceBuffer = nullptr,
                const cauldron::Buffer* pLeafInstanceBuffer = nullptr)
    {
        static bool sLoggedOnce = false;
        if (!sLoggedOnce)
        {
            cauldron::CauldronWarning(L"[IvyRenderIndirect] Render() entered (ExecuteIndirect no-op)");
            sLoggedOnce = true;
        }

        if (!m_pIndirectWorkload || !pArgumentBuffer || !m_pPipelineObject)
            return;

        // Use the passed command list from IvyRenderModule to ensure consistency
        if (!pCmdList)
            return;

        // Set pipeline state
        cauldron::SetPipelineState(pCmdList, m_pPipelineObject);

        // Set primitive topology to triangle list
        cauldron::SetPrimitiveTopology(pCmdList, cauldron::PrimitiveTopology::TriangleList);

        // Update ViewProjection matrix in our own ParameterSet
        cauldron::BufferAddressInfo viewProjBufferInfo = cauldron::GetDynamicBufferPool()->AllocConstantBuffer(sizeof(Mat4), &viewProjectionMatrix);
        m_pParameterSet->UpdateRootConstantBuffer(&viewProjBufferInfo, 0);

        // Note: Root constants will be updated per draw call using UpdateRootConstantBuffer for better performance

        // Bind instance buffers once (if not already bound)
        if (!m_instanceBuffersBound && (pLeafInstanceBuffer || pStemInstanceBuffer))
        {
            if (pLeafInstanceBuffer)
                m_pParameterSet->SetBufferSRV(pLeafInstanceBuffer, 0);  // t0: Leaf instance buffer
            if (pStemInstanceBuffer)
                m_pParameterSet->SetBufferSRV(pStemInstanceBuffer, 1);  // t1: Stem instance buffer
                
            m_instanceBuffersBound = true;
            cauldron::CauldronWarning(L"[IvyRenderIndirect] Instance buffers bound to t0 and t1");
        }

        // Note: ParameterSet will be bound for each draw call after setting instance_buffer_index

        // Note: Argument buffer is now initialized by IvyRenderModule each frame
        // and will be modified by work graph compute nodes via AtomicAdd operations

        // Execute leaf geometry if valid
        if (ivyLeafSurfaceIndex >= 0 && pVertexBuffers && pIndexBuffers && pSurfaceBuffer)
        {
            const Surface_Info& leafSurfaceInfo = (*pSurfaceBuffer)[ivyLeafSurfaceIndex];
            
            // Set vertex and index buffers for leaf geometry
            cauldron::BufferAddressInfo positionBufferInfo = (*pVertexBuffers)[leafSurfaceInfo.position_attribute_offset]->GetAddressInfo();
            cauldron::BufferAddressInfo normalBufferInfo = (*pVertexBuffers)[leafSurfaceInfo.normal_attribute_offset]->GetAddressInfo();
            
            cauldron::BufferAddressInfo vertexBuffers[2] = { positionBufferInfo, normalBufferInfo };
            cauldron::SetVertexBuffers(pCmdList, 0, 2, vertexBuffers);

            cauldron::BufferAddressInfo indexBufferInfo = (*pIndexBuffers)[leafSurfaceInfo.index_offset]->GetAddressInfo();
            cauldron::SetIndexBuffer(pCmdList, &indexBufferInfo);

            uint32_t leafBufferIndex[4] = {0, 0, 0, 0};  // Padded to 16 bytes alignment
            cauldron::BufferAddressInfo leafIndexBufferInfo = cauldron::GetDynamicBufferPool()->AllocConstantBuffer(sizeof(uint32_t) * 4, leafBufferIndex);
            m_pParameterSet->UpdateRootConstantBuffer(&leafIndexBufferInfo, 1);  // Set instance_buffer_index = 0 for leaf (b1)
            m_pParameterSet->Bind(pCmdList, m_pPipelineObject);                  // Bind with updated constant

            // Execute first draw command (leaf) using offset 0
            cauldron::ExecuteIndirect(pCmdList, m_pIndirectWorkload, pArgumentBuffer, 1 /*drawCount*/, 0 /*offset*/);
            cauldron::CauldronWarning(L"[IvyRenderIndirect] Executed leaf geometry with buffer index 0");
        }

        // Execute stem geometry if valid
        if (ivyStemSurfaceIndex >= 0 && pVertexBuffers && pIndexBuffers && pSurfaceBuffer)
        {
            const Surface_Info& stemSurfaceInfo = (*pSurfaceBuffer)[ivyStemSurfaceIndex];
            
            // Set vertex and index buffers for stem geometry
            cauldron::BufferAddressInfo positionBufferInfo = (*pVertexBuffers)[stemSurfaceInfo.position_attribute_offset]->GetAddressInfo();
            cauldron::BufferAddressInfo normalBufferInfo = (*pVertexBuffers)[stemSurfaceInfo.normal_attribute_offset]->GetAddressInfo();
            
            cauldron::BufferAddressInfo vertexBuffers[2] = { positionBufferInfo, normalBufferInfo };
            cauldron::SetVertexBuffers(pCmdList, 0, 2, vertexBuffers);

            cauldron::BufferAddressInfo indexBufferInfo = (*pIndexBuffers)[stemSurfaceInfo.index_offset]->GetAddressInfo();
            cauldron::SetIndexBuffer(pCmdList, &indexBufferInfo);

            uint32_t stemBufferIndex[4] = {1, 0, 0, 0};  // Padded to 16 bytes alignment
            cauldron::BufferAddressInfo stemIndexBufferInfo = cauldron::GetDynamicBufferPool()->AllocConstantBuffer(sizeof(uint32_t) * 4, stemBufferIndex);
            m_pParameterSet->UpdateRootConstantBuffer(&stemIndexBufferInfo, 1);  // Set instance_buffer_index = 1 for stem (b1)
            m_pParameterSet->Bind(pCmdList, m_pPipelineObject);                  // Bind with updated constant

            // Execute second draw command (stem) using offset sizeof(DrawIndexedArgs)
            cauldron::ExecuteIndirect(pCmdList, m_pIndirectWorkload, pArgumentBuffer, 1 /*drawCount*/, sizeof(DrawIndexedArgs) /*offset*/);
            cauldron::CauldronWarning(L"[IvyRenderIndirect] Executed stem geometry with buffer index 1");
        }
    }
};
