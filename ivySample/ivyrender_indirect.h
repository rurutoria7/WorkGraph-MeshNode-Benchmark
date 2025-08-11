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
    // Dummy indirect args layout matching D3D12_DRAW_INDEXED_ARGUMENTS
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

        DrawIndexedArgs dummyArgs{};

        cauldron::BufferDesc argsDesc = cauldron::BufferDesc::Data(L"Ivy_IndirectArgs", sizeof(DrawIndexedArgs), sizeof(uint32_t));
        m_pArgsBuffer                 = cauldron::Buffer::CreateBufferResource(&argsDesc, cauldron::ResourceState::CopyDest);
        m_pArgsBuffer->CopyData(&dummyArgs, sizeof(dummyArgs));

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

        // Setup input layout for position only
        std::vector<cauldron::InputLayoutDesc> vertexInputLayout = {
            cauldron::InputLayoutDesc(cauldron::VertexAttributeType::Position, cauldron::ResourceFormat::RGB32_FLOAT, 0, 0)
        };
        psoDesc.AddInputLayout(vertexInputLayout);

        // Setup rasterizer state - use same as work graph (backface culling)
        cauldron::RasterDesc rasterDesc;
        rasterDesc.CullingMode = cauldron::CullMode::Back;
        rasterDesc.FrontCounterClockwise = true;
        psoDesc.AddRasterStateDescription(&rasterDesc);

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

    void Render(const std::vector<const cauldron::Buffer*>* pVertexBuffers = nullptr, 
                const std::vector<const cauldron::Buffer*>* pIndexBuffers = nullptr,
                int ivyLeafSurfaceIndex = -1,
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

        // Set up vertex and index buffers for leaf geometry if available
        if (pVertexBuffers && pIndexBuffers && pSurfaceBuffer && ivyLeafSurfaceIndex >= 0 && 
            ivyLeafSurfaceIndex < static_cast<int>(pSurfaceBuffer->size()))
        {
            const Surface_Info& leafSurfaceInfo = (*pSurfaceBuffer)[ivyLeafSurfaceIndex];
            
            // Use position_attribute_offset to get the correct position buffer
            if (leafSurfaceInfo.position_attribute_offset >= 0 && 
                leafSurfaceInfo.position_attribute_offset < static_cast<int>(pVertexBuffers->size()) &&
                leafSurfaceInfo.index_offset >= 0 && 
                leafSurfaceInfo.index_offset < static_cast<int>(pIndexBuffers->size()))
            {
                // Get position vertex buffer using the offset from surface info
                cauldron::BufferAddressInfo vertexBufferInfo = (*pVertexBuffers)[leafSurfaceInfo.position_attribute_offset]->GetAddressInfo();
                cauldron::SetVertexBuffers(pCmdList, 0, 1, &vertexBufferInfo);

                // Get index buffer using the offset from surface info
                cauldron::BufferAddressInfo indexBufferInfo = (*pIndexBuffers)[leafSurfaceInfo.index_offset]->GetAddressInfo();
                cauldron::SetIndexBuffer(pCmdList, &indexBufferInfo);

                // Update argument buffer with real geometry data
                DrawIndexedArgs realArgs{};
                realArgs.IndexCountPerInstance = leafSurfaceInfo.num_indices;
                realArgs.InstanceCount = 1;  // Render one instance
                realArgs.StartIndexLocation = 0;
                realArgs.BaseVertexLocation = 0;
                realArgs.StartInstanceLocation = 0;
                
                // Update argument buffer with real data
                m_pArgsBuffer->CopyData(&realArgs, sizeof(realArgs));

                cauldron::CauldronWarning(L"[IvyRenderIndirect] Leaf geometry buffers bound: position_offset=%d, index_offset=%d, indices=%d", 
                                        leafSurfaceInfo.position_attribute_offset, leafSurfaceInfo.index_offset, leafSurfaceInfo.num_indices);
            }
        }

        if (!m_argsReady)
        {
            cauldron::Barrier barrier = cauldron::Barrier::Transition(
                m_pArgsBuffer->GetResource(), cauldron::ResourceState::CopyDest, cauldron::ResourceState::IndirectArgument);
            cauldron::ResourceBarrier(pCmdList, 1, &barrier);
            m_argsReady = true;
        }

        cauldron::ExecuteIndirect(pCmdList, m_pIndirectWorkload, m_pArgsBuffer, 1 /*drawCount*/, 0 /*offset*/);
    }
};

/*
## Referencing Geometry

std::vector<const cauldron::Buffer*> m_VertexBuffers;
std::vector<const cauldron::Buffer*> m_IndexBuffers;

## Goal 1: Use ExecuteIndirect Draw a Leaf

1. [✅] Run Execute Indirect without error
    - [✅] all arguments should be dummy → Now using real arguments

2. [✅] Render Leaf's geometry
    - [✅] IA should find position & indices
        - [✅] should reference to surfaces infomation
    - [✅] Create Pipeline state
        - [✅] bind VS and PS
            - [✅] VS don't need to output transformted positions
                - (constant buffer is not binded)
            - [✅] PS just output red
                - (normal, texture, texcoord is not binded)
    - [✅] PSO -> Input Layout (Position, Normal, Texcoord0)
    - [x] RS, OM, ... should be set up
        - [✅] Render target, depth buffer formats configured
        - [✅] Rasterizer state configured (backface culling)
        - References IvyRenderModule::Execute() for barriers setup
    - [✅] Argument buffer should be set correctly
        - [✅] Uses real geometry data: IndexCount, InstanceCount=1

3. Transform should be correct (add Constant Buffer View)
    - Root signature, Parameter, descriptor should be bind

4. PS output Normal as color
    - Add vertex attribute, passed to PS

5. PS output Albedo as color
    - Bind Albedo texture SRV

6. Align Pixel Shader to original Implementation

7. Use ExecuteIndirect Draw a Branch instance
    - Implement the same steps as Goal 1 for Branch instances
    - Argument buffer should be extend

*/