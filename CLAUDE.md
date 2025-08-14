# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Language Preference

**請使用中文回應所有問題和對話。** Always respond in Traditional Chinese (繁體中文) when communicating with the user.

## Build and Development Commands

**Initial Setup:**
```bash
cmake -B build .
```
This downloads FidelityFX SDK, media files, Agility SDK, and DXC, then configures the project.

**Open Visual Studio:**
```bash
cmake --open build
```

**Build Target:** Build and run the `IvySample` project in Visual Studio.

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

**Resource Binding:** Uses Cauldron `ParameterSet` for resource binding and management. Work graph and ExecuteIndirect share the same root signature and parameter set for consistency.

**Rendering Pipeline:** 
1. Barrier setup → BeginRaster 
2. Camera constant buffer upload/bind via ParameterSet
3. SetProgram + DispatchGraph (work graph mesh nodes generate transforms)
4. ExecuteIndirect with dual geometry (leaf and stem)
5. EndRaster

**Shader Organization:** Shaders are stored in `shaders/` directory but referenced as `Shaders\\` in ShaderCompiler. Keep folder named `shaders` and reference files by name in code.

### Important Integration Notes

- ExecuteIndirect uses shared work graph root signature and parameter set
- Uses Cauldron `Barrier` helpers for render target transitions
- Geometry binding follows pattern: Begin/End raster with `BeginRaster`/`EndRaster` and `SetViewportScissorRect`
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

**Resource Management:** All resource binding goes through Cauldron `ParameterSet`. ExecuteIndirect shares the work graph's root signature and parameter set for consistent constant buffer access.

**Geometry Binding Pattern:**
1. Set vertex buffers: Position (slot 0) + Normal (slot 1) 
2. Set index buffer via Surface_Info offset
3. Execute draw commands with argument buffer offsets

**Valid Warnings:** `[IvyRenderIndirect] Vertex/Index buffers bound for leaf geometry` - this is expected behavior for the dual geometry setup.