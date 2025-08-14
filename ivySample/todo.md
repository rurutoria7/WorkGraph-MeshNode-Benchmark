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

## Goal 2: Instance Draw

1. Draw 2 Instance
    - argument buffer should update instance count
    - instance buffer is created
    - Bind Instance Buffer as SRV
        - Modify Root Signature
            - **What Slot?**
        - Modify Parametere Set
        - Add binding command
    - Get Attribute in HLSL

```
這是因為我們嘗試共用 root signature。 root signature 有分成 compute 和 graphics type。
我們應該把 root signature 分開才對。
制定計劃，為 ExecuteIndirect 的 pass 獨立出一個 root signature。
```

2. Draw 2 Instance with Stem & Leaf (both)
    - 2 instance buffer


## Goal 3: Compute Node Write Instance Buffer & Args Buffer

1. Only Modify the Instance Count
    - from work graph's last compute node, AtomicAdd 

## Resource

Descriptor Heap
Root Signature
Shader Register

## Goal: Draw Better

1. PS output Albedo as color
    - Bind Albedo texture SRV

2. Align Pixel Shader to original Implementation
*/