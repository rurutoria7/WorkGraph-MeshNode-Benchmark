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

## Goal 3: Modify from Work Graph 

1. [solved] Bug: IvyBranch Shader Cannot Change "Argument"

- 整理 Arg 的 resource state
    - First: Skip IA to CD
    - Following: IA --> CD --> (Init) --> UA --> (WG) --> IA --> (ID)

```c
(WG)----+ 2. write
        |
        V
[Arg Buffer]     <------ (CPU) 1. init
[Inst Buffer]
   ^
   |    3. read
(I.D.)
```
- 猜測：並不是沒看到，只是因為數量設定不對
    - Per frame Initialize: 一排十個，渲染 20 個
    - Work Graph: Atomic Add + 1
    - 結果, 有 Atomic Add 就不行！

- Barrier 的範圍是 CmdList 嗎？
    - 排除
- Barrier 用錯了嗎？所以不能保證順序。
    - 排除

- profile
    - 最後 Leaf 的 Draw call 是 Draw(instance count = 2436), 4359
    - 最後 Stem 的 Draw call 是 Draw(instance count = 2436), 

2. [solved] Bug: instance count 數量不穩定
    - 有時候是 2436, 2388

3. 有時候會整個閃一下
    - 在 In Entry Node，把 InstanceCount 設定成 0，把 IndicesCount 設定成正確的數值
        1. Entry Node 在 area.hlsl
        2. argument buffer 綁定直接移動到 raytracing.hlsl，所以 ivy.hlsl 和 area.hlsl 都 access 的到
        3. 列出修改的計劃，跟我確認
        4. 還要移除 argument buffer 的 COPY_DEST Resource state transition
            - Init state: Indirect Argument
            - Every Frame: Indirect Argument --> Unordered Access --> (Work Graph) --> Indirect Argument --> (Execute Indirect)
        5. 改成 Atomic Add

3. Try to write transform
    - Work Graph should bind "leaf instance buffer"
        - modify root signature
        - 
    - Try to move all instance by 2, y axis
        - every group's first thread do atomic add, and it should translate those instance it just allocated.

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