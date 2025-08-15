# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Preference

**請使用中文回應所有問題和對話。** Always respond in Traditional Chinese (繁體中文) when communicating with the user.

**我會親自執行和建置程式嗎，所以只要寫 code 就好了**

## Project Architecture

This is a D3D12 Work Graphs sample demonstrating procedural ivy generation and ExecuteIndirect rendering, built on the FidelityFX SDK (Cauldron framework).

### Core Components

- **`main.cpp`**: Application entry point that boots Cauldron Framework, parses `config/ivysampleconfig.json`, registers `IvyRenderModule`, and enables experimental D3D12 features
- **`ivyrendermodule.h/.cpp`**: Main render orchestration module handling frame execution, work graph setup, and ExecuteIndirect integration
- **`ivyrender_indirect.h`**: ExecuteIndirect implementation managing argument buffers for dual draw commands (leaf/stem geometry) with vertex/index buffer binding
- **`shadercompiler.h/.cpp`**: DXC wrapper for HLSL 2021 compilation with column-major matrices, targeting `lib_6_9` and `ps_6_9`
- **`shaders/`**: HLSL shaders including work graph compute nodes and ExecuteIndirect vertex/pixel shaders
- **`config/ivysampleconfig.json`**: Application configuration file

### Key Architectural Patterns

**混合 API 層次使用:**
- 高層抽象 (Cauldron): ParameterSet, Buffer, Texture 管理
- 原生 D3D12 API: Work graph 狀態物件建立和調度  
- 混合模式: 資源建立用 Cauldron，狀態管理用 D3D12

**創新的混合渲染管線:**
Work Graph → ExecuteIndirect 鏈條:
1. Work graph 計算節點生成程序化幾何變換
2. 通過共享 argument buffer 傳遞實例數量 
3. ExecuteIndirect 使用相同緩衝區執行雙重幾何渲染

**資源建立層次架構:**
1. **Work Graph 資源群組**: 共享根簽名、參數集、狀態物件、背景記憶體
2. **ExecuteIndirect 專用資源**: 雙重繪製指令緩衝區、StructuredBuffer 實例資料
3. **RT Info Tables**: 材質、表面、幾何緩衝區的索引表

**三階段 Barrier 管理:**
```cpp
// 1. GBuffer 準備: Shader Resource → Render Target
// 2. Argument Buffer: Copy Dest → Unordered Access (Work Graph)  
// 3. Argument Buffer: Unordered Access → Indirect Argument (ExecuteIndirect)
```

**Shader Organization:** Shaders are stored in `shaders/` directory but referenced as `Shaders\\` in ShaderCompiler. Keep folder named `shaders` and reference files by name in code.

### Advanced Resource Management Patterns

**資源共享但邏輯分離策略:**
- Work graph 和 ExecuteIndirect 共享 argument buffer
- 使用完全獨立的根簽名和參數集
- 允許各自最佳化資源綁定

**GBuffer 渲染管線架構 (四重渲染目標 + 深度):**
- Albedo RT (m_pGBufferAlbedoOutput)
- Normal RT (m_pGBufferNormalOutput) 
- PBR RT (m_pGBufferAoRoughnessMetallicOutput)
- Motion Vector RT (m_pGBufferMotionOutput)
- Depth Buffer (m_pGBufferDepthOutput)

**ExecuteIndirect StructuredBuffer 最佳化:**
```cpp
// 避開 Cauldron 限制: 不支援 D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
execIndirectRootSigDesc.AddBufferSRVSet(0, ShaderBindStage::Vertex, 1); // Instance buffer SRV
// 使用 StructuredBuffer + SRV 替代傳統 instance data
```

### Important Integration Notes

- **資源共享模式**: argument buffer 在 work graph (UAV) 和 ExecuteIndirect (IndirectArgument) 間共享
- **D3D12 Work Graphs 進階用法**: Mesh Node 到 Compute Node 遷移，使用 graphics state 進行全域根簽名
- **雙重幾何綁定**: 葉子 (offset 0) + 莖幹 (offset sizeof(DrawIndexedArgs))，不同 Surface_Info 偏移
- **錯誤處理策略**: 表面資料未載入時優雅降級，跳過參數緩衝區初始化但繼續渲染管線
- Valid warning to ignore: `[IvyRenderIndirect] Vertex/Index buffers bound for leaf geometry`
- Always reference `FidelityFX-SDK/` for API usage patterns and examples

### File Structure

```
ivySample/
├── main.cpp                    # Application entry point
├── ivyrendermodule.h/.cpp      # Main render module
├── ivyrender_indirect.h        # ExecuteIndirect implementation
├── shadercompiler.h/.cpp       # HLSL compilation wrapper
├── shaders/                    # HLSL shader files
│   ├── ivycommon.h            # Shared constants/structures
│   ├── ivyleaf_indirect.hlsl  # ExecuteIndirect shaders
│   └── *.hlsl                 # Work graph and other shaders
├── config/
│   └── ivysampleconfig.json   # App configuration
└── FidelityFX-SDK/            # Framework source code
```

## Development Prerequisites

- CMake 3.17+
- Visual Studio 2019+
- Windows 10 SDK 10.0.18362.0+
- Vulkan SDK 1.3.239+ (Cauldron build dependency)
- Mesh node compatible driver

## Controls and UI

- Left mouse: Select ivy root/area
- 3D gizmo: Manipulate translation/rotation/scale
- Right mouse: Toggle arc-ball/WASD camera modes
- F1/F2/F3: Toggle UI panels
- ESC: Quit
- Alt-Enter: Toggle fullscreen

## Project Context

This sample demonstrates the migration from work graph mesh nodes to ExecuteIndirect rendering while maintaining procedural ivy generation through work graph compute nodes. The goal is to render stem and leaf instances using ExecuteIndirect instead of mesh nodes, while preserving the work graph-generated transforms.

**Instance Buffer Implementation:** Use StructuredBuffer approach instead of Input Layout for per-instance data. Cauldron Framework does not support D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA - it hardcodes all vertex inputs as D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA. StructuredBuffer provides the standard, flexible solution for instance data binding through Root Signature SRV slots.

## Work Graph to ExecuteIndirect Migration

**Current State:** The project successfully renders dual geometry (leaf and stem) using ExecuteIndirect commands while maintaining work graph compute nodes for procedural generation.

**Key Migration Points:**
- Work graph compute nodes generate transforms procedurally
- ExecuteIndirect replaces mesh nodes for rendering instances
- Shared root signature and parameter set between work graph and ExecuteIndirect ensures consistency
- Dual draw commands handle separate leaf and stem geometry with different Surface_Info offsets

## Development Guidelines

**Shader Compilation:** Use DXC with HLSL 2021, targeting `lib_6_9` and `ps_6_9` with column-major matrices. Shaders are stored in `shaders/` but referenced as `Shaders\\` in ShaderCompiler.

**Resource Management 深度分析:**

**Render Module 生命週期和狀態管理:**
- **初始化階段**: 四層架構 (紋理參考 → Work Graph → ExecuteIndirect → UI 整合)
- **執行階段狀態機**: Barrier 設置 → 工作圖準備 → 程序化生成 → ExecuteIndirect 執行
- **內容載入生命週期**: 動態資源建構，特殊處理 Ivy Stem/Leaf 表面索引識別

**混合綁定模式:**
```cpp
// Work Graph 使用複雜的參數集
m_pWorkGraphParameterSet->SetBufferUAV(m_pArgumentBuffer, 0);           // u0: 參數緩衝區
m_pWorkGraphParameterSet->SetAccelerationStructure(GetScene()->GetASManager()->GetTLAS(), 0); // t0: TLAS

// ExecuteIndirect 使用獨立的簡化參數集  
m_pParameterSet->SetRootConstantBufferResource(..., 0);  // b0: ViewProjection
m_pParameterSet->SetBufferSRV(pLeafInstanceBuffer, 0);   // t0: 實例緩衝區
```

**三階段資源流程管線:**
1. **程序化生成階段**: IvyBranchRecord/IvyAreaRecord → 計算節點 → 修改 argument buffer InstanceCount
2. **幾何綁定階段**: 設置葉子/莖幹幾何的不同 Surface_Info 偏移
3. **間接渲染階段**: 雙重繪製指令執行，不同實例緩衝區

**Geometry Binding Pattern:**
1. Set vertex buffers: Position (slot 0) + Normal (slot 1) 
2. Set index buffer via Surface_Info offset
3. Execute draw commands with argument buffer offsets (leaf: offset 0, stem: sizeof(DrawIndexedArgs))

**Valid Warnings:** `[IvyRenderIndirect] Vertex/Index buffers bound for leaf geometry` - this is expected behavior for the dual geometry setup.