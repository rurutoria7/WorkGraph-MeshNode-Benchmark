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
    - [bug] Try to move all instance to (0, 2, 0)
        - workgraph bind instance buffer 有問題 [x]
            - 2 buffer creation
                - CopyData 把資料改掉了
        - 修改 instance buffer 時，邏輯有問題
            - [ ] 每次都是後面的 instance沒有被改到！
        - barrier 有問題
            - DeviceMemory, Fence, globallycoherent, ...
        - 讀取 instance buffer 渲染時有問題
    - [bug] 如果每個 thread group +1 argument 會導致每次渲染的數量不一樣
        - [x] 可能 coalescing 不太一定，但是如果加 output instance count 就一樣了

4. [x] debug: 可以寫 original transform，但是有 bug:
    - bug:
        1. stem 的 transform 應該是對的，leaf 的 transform 大致上正確但是都有一點偏移。
        2. instance 會閃爍
        3. 有很大部分的 instance，從某個編號以後，就沒有被修改到。
    - 檢查：
        1. [ ] 在 @ivyleafrender.hlsl 裡面， leaf 套用 transform 的邏輯，是不是和我們目前一致
        2. [x] barrier 是否設置正確，或有其他因素導致 barrier 沒有正常工作
        3. [x] InterlockedAdd 的使用方式是否正確
        4. [ ] @ivy.hlsl 寫入 instance 的邏輯錯誤
    - 一個重要的線索是，leaf 是錯的但是 stem 是正常的。那如果單獨修改 leaf 呢？
    - 會不會是初始化執行了兩次？取消一個 input record

5. [ ] 解決 bug: 懷疑 SetBufferSRV 沒有如預期中執行。
    - sol:
        1. 使用 constant buffer，传入一个 uint，叫做 "instance_buffer_index", 其中 leaf 是 0， stem 是 1
        2. 在 root signature 宣告两个 SRV slot
        3. 在第一次传入 instance buffer 的时候，使用 SetBufferSRV 绑定到 t0, t1
        4. 在 HLSL 使用 descriptor array 来宣告接住 2 个 buffer
        5. 在 HLSL 内部使用的时候，用 instance_buffer_index 来 index 
        6. 不用，在内部记录是否有绑定过就可以
    - [ ] how to update root argument
    
    
```
RootSig [CBV: b0, Dtable(SRV: t0)]
ParameterSet:
    - Dheap[SRV]
    - RootConstant
    + SetSRV(hlsl_idx, buffer_address)
    + BindAll()

SetSRV() --write--> [Dheap] <--read-- BindAll() --write--> [cmdList.rootParam]
```

```
AtomicAdd(argument[LEAF].InstanceCount, outputStemCount, orignalCount);
if isFirstThreadInGroup():
    for i = 0, 1, ..., outputStemCount:
        LeafInstance[i + originalCount].transform *= TranslateMatrix(0, 2, 0)
```

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