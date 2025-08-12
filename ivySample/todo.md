/*
## Referencing Geometry

std::vector<const cauldron::Buffer*> m_VertexBuffers;
std::vector<const cauldron::Buffer*> m_IndexBuffers;

## [x] Goal 1: Use ExecuteIndirect Draw a Leaf

1. [x] Run Execute Indirect without error
    - [x] all arguments should be dummy → Now using real arguments

2. [x] Render Leaf's geometry
    - [x] IA should find position & indices
        - [x] should reference to surfaces infomation
    - [x] Create Pipeline state
        - [x] bind VS and PS
            - [x] VS don't need to output transformted positions
                - (constant buffer is not binded)
            - [x] PS just output red
                - (normal, texture, texcoord is not binded)
    - [x] PSO -> Input Layout (Position, Normal, Texcoord0)
    - [x] RS, OM, ... should be set up
        - [x] Render target, depth buffer formats configured
        - [x] Rasterizer state configured (backface culling)
        - References IvyRenderModule::Execute() for barriers setup
    - [x] Argument buffer should be set correctly
        - [x] Uses real geometry data: IndexCount, InstanceCount=1    

3. [x] Transform should be correct (add Constant Buffer View)
    - the main idea is, ivyleaf_indirect should have reference to constant buffer, in raytracing.hlsl
    - use this API to bind parameters and root signature
        -     m_pWorkGraphParameterSet->Bind(pCmdList, nullptr);
    - the transformation can reference to ivyleafrender.hlsl
    - 如果有任何沒有考慮到的可以暫停下來確認

4. PS output Normal as color
    - Add vertex attribute, passed to PS

5. Draw 1 stem also
    - [x] Argument buffer, add one element
        - getting instance count can reference to drawing leaf
    - [ ] 

## Goal 2: Instance Draw

1. Draw 2 Instance
    - Create Instance buffer, which contains 2 instance data
    - instance data just contains transform matrix: float4x4
    - argument buffer should update instance count
    - instance buffer is filled with CPU: 
        - first instance no transform (identity)
        - second instance have small offset


## Goal 3

## Goal: Draw Better

1. PS output Albedo as color
    - Bind Albedo texture SRV

2. Align Pixel Shader to original Implementation
*/