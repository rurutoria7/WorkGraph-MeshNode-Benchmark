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
    cauldron::Buffer*           m_pInstanceBuffer   = nullptr;
    cauldron::IndirectWorkload* m_pIndirectWorkload = nullptr;
    cauldron::RootSignature*    m_pRootSignature    = nullptr;  // Own Graphics Root Signature
    cauldron::ParameterSet*     m_pParameterSet     = nullptr;  // Own ParameterSet
    cauldron::PipelineObject*   m_pPipelineObject   = nullptr;
    bool                        m_argsReady         = false;
    uint32_t                    m_maxInstances      = 1000;

    ~IvyRenderIndirect()
    {
        if (m_pArgsBuffer)
            delete m_pArgsBuffer;
        if (m_pInstanceBuffer)
            delete m_pInstanceBuffer;
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

        // Create argument buffer for both leaf and stem (two draw commands)
        DrawIndexedArgs dummyArgs[2] = {};

        cauldron::BufferDesc argsDesc = cauldron::BufferDesc::Data(L"Ivy_IndirectArgs", sizeof(DrawIndexedArgs) * 2, sizeof(uint32_t));
        m_pArgsBuffer = cauldron::Buffer::CreateBufferResource(&argsDesc, cauldron::ResourceState::CopyDest);
        m_pArgsBuffer->CopyData(dummyArgs, sizeof(dummyArgs));

        // Create instance buffer as StructuredBuffer
        cauldron::BufferDesc instanceDesc = cauldron::BufferDesc::Data(L"Ivy_InstanceBuffer", sizeof(IvyInstanceData) * m_maxInstances, sizeof(IvyInstanceData));
        m_pInstanceBuffer = cauldron::Buffer::CreateBufferResource(&instanceDesc, cauldron::ResourceState::CopyDest);

        // Initialize instances for both leaf and stem (4 total instances)
        IvyInstanceData allInstances[4];
        
        // Leaf instances (indices 0-1): positioned on the left side  
        allInstances[0].transform = Mat4::translation(Vec3(-3.0f, 0.0f, 0.0f));  // Leaf instance 0
        allInstances[1].transform = Mat4::translation(Vec3(-1.0f, 0.0f, 0.0f));  // Leaf instance 1
        
        // Stem instances (indices 2-3): positioned on the right side
        allInstances[2].transform = Mat4::translation(Vec3(1.0f, 0.0f, 0.0f));   // Stem instance 0  
        allInstances[3].transform = Mat4::translation(Vec3(3.0f, 0.0f, 0.0f));   // Stem instance 1
        
        // Upload instance data to buffer
        m_pInstanceBuffer->CopyData(allInstances, sizeof(allInstances));

        // Create independent Graphics Root Signature for ExecuteIndirect
        cauldron::RootSignatureDesc execIndirectRootSigDesc;
        execIndirectRootSigDesc.AddConstantBufferView(0, cauldron::ShaderBindStage::Vertex, 1);        // ViewProjection CBV
        execIndirectRootSigDesc.AddBufferSRVSet(0, cauldron::ShaderBindStage::Vertex, 1);             // Instance buffer SRV
        execIndirectRootSigDesc.m_PipelineType = cauldron::PipelineType::Graphics;
        
        m_pRootSignature = cauldron::RootSignature::CreateRootSignature(L"ExecuteIndirect_RootSignature", execIndirectRootSigDesc);
        
        // Create ParameterSet for this Root Signature
        m_pParameterSet = cauldron::ParameterSet::CreateParameterSet(m_pRootSignature);
        
        // Initialize root constant buffer resource - Shader Register b0
        m_pParameterSet->SetRootConstantBufferResource(cauldron::GetDynamicBufferPool()->GetResource(), sizeof(Mat4), 0);
        
        // Bind instance buffer to SRV - Shader Register t0  
        m_pParameterSet->SetBufferSRV(m_pInstanceBuffer, 0);

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

    void Render(const Mat4& viewProjectionMatrix,
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

        // Update ViewProjection matrix in our own ParameterSet
        cauldron::BufferAddressInfo viewProjBufferInfo = cauldron::GetDynamicBufferPool()->AllocConstantBuffer(sizeof(Mat4), &viewProjectionMatrix);
        m_pParameterSet->UpdateRootConstantBuffer(&viewProjBufferInfo, 0);
        
        // Bind our own ParameterSet with ViewProjection CBV and Instance Buffer SRV
        m_pParameterSet->Bind(pCmdList, m_pPipelineObject);

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
                    drawCommands[0].InstanceCount = 2;  // Render 2 leaf instances
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
                    drawCommands[1].InstanceCount = 2;  // Render 2 instances
                    drawCommands[1].StartIndexLocation = 0;
                    drawCommands[1].BaseVertexLocation = 0;
                    drawCommands[1].StartInstanceLocation = 2;  // Start from instance index 2
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
