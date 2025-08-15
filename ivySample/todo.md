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

1. [x] Draw 2 Instance
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

2. [x] Draw 2 Instance with Stem & Leaf (both)
    - 2 instance buffer
        - On Init
            - [ ] Create another instance buffer
        - Execute
            - Add a binding


## Goal 3: Compute Node Write Instance Buffer & Args Buffer

1. Only Modify the Instance Count
    - from work graph's last compute node, AtomicAdd "Argument Buffer"
        - Buffer 
            - [ ] Argument Buffer should be member of "ivyrendermodule"
            - [ ] Initialize: Every Frame it should be initialized with correct e.g. VertexCount and 0 in Instance Count
                - use command to copy
            - [ ] Resource Barrier:
                1. not first time: Indirect --> Copy Dest
                2. Initialze argument buffer with Copy Command
                3. CopyDest --> Unorded Access
                4. Dispatch Graph()
                5. Unordered Access --> Indirect
        - Referencing in HLSL
            - [ ] StructuredBuffer<IndirectCommand>, so IndirectCommand structure's declaration should be moved to "ivycommon.h"
            - [ ] in "ivy.hlsl" group 0-3: leaf add 1, leaf add 1, stem add 1, stem add 1, totaly 4 instance
        - Root Signature
            - Should Modify Work Graph's Root Signature
            - Instead of Native API, Caldron's API ParameterSet should be used.
            1. [ ] Figure out available Register Index
            2. [ ] Modify Root Signature creation, add a UAV
            3. [ ] Bind argument buffer once
    - We should see 4 instance on the screen


2. Atomic Add Instance Count
    - Fill instance_buffer by CPU, on Init
        - Fill with dummy values, let instance are arranged in a rows and rows
            - the space between every instance can be like e.g. 0.5
    - In ivy.hlsl --> IvyBranch(), every Wave should Add Instance Count by 1
        - So that we can see Atomic Add is working as we expected

```
做一個調整，就是把 instance transform 往上移動 1.
然後把 instance count 調整到 10000

把 atomic add 放回來
把一個 row 改成 1000 instance

bug: 加上 atomic add 之後，instance 還是不見了。
檢查方向：
- resource barrier 問題
- 先假設 ivyBranch 都有執行到
- 也可以提出一些實驗來排除可能性
- 


把取得的知識，還有新做的更改，總結到 CLAUD.md。
另外檢查 CLAUD.md 有沒有 out of date 的內容。
```


- update CLAUDE.md

```
understand these files, especially
1. resource creation, binding, barrier
2. life cycle of this render module
3. render pass, what resource is involved
4. API Usage

key files are #ivyrendermodule #ivyrender_indirect
```

3. Try to write transform

4. Write correct transform

## Goal 4: Draw Better

1. PS output Albedo as color
    - Bind Albedo texture SRV

2. Align Pixel Shader to original Implementation

## Goal 5: Consider Output Limit

## Resource

Descriptor Heap
Root Signature
Shader Register


*/