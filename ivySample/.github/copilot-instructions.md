# Copilot instructions: WorkGraphsIvyGenerationSample

This sample builds on the FidelityFX SDK (Cauldron) to demonstrate D3D12 Work Graphs (mesh nodes) and a minimal ExecuteIndirect path.

## Modify Goal

- The module generate Stem & Leaf's transform procedurally by work graph compute node.
- Then it render with work graph mesh node
- Rather than mesh node, we want to render these instances using ExecuteIndirect.

## Architecture (what talks to what)
- `main.cpp`: boots Cauldron `Framework`, parses `configs/ivysampleconfig.json`, registers `IvyRenderModule`, enables experimental D3D12 features.
- `ivyrendermodule.*`: frame orchestration.
  - `InitTextures()` sets GBuffer views via Cauldron helpers.
  - `InitWorkGraphProgram()` builds root signature and a state object with a Work Graph; compiles `shaders/*.hlsl` with `ShaderCompiler`.
  - `Execute(...)`: barrier setup → BeginRaster → upload/bind camera CB via `ParameterSet` → `SetProgram` + `DispatchGraph` (mesh nodes) → indirect draw → EndRaster.
- `ivyrender_indirect.h`: tiny ExecuteIndirect using a single zeroed `D3D12_DRAW_ARGUMENTS`.
- `shadercompiler.*`: DXC wrapper (HLSL 2021, column-major), loads from `Shaders/`, compiles `lib_6_9` and `ps_6_9`.
- `FidelityFX-SDK/` the framework's source code, for referencing.

## Project-specific conventions
- Bind resources via `ParameterSet`;
- Use Cauldron `Barrier` helpers for RT transitions; Begin/End raster with `BeginRaster`/`EndRaster` and `SetViewportScissorRect`.
- Shaders live in `shaders/` but `ShaderCompiler` loads from `Shaders\\` path; keep folder named `shaders` and reference files by name in code.
- This is valid, so just ignore the error: `cauldron::CauldronWarning(L"[IvyRenderIndirect] Vertex/Index buffers bound for leaf geometry");`

Key files: `main.cpp`, `ivyrendermodule.h/.cpp`, `ivyrender_indirect.h`, `shadercompiler.h/.cpp`, `config/ivysampleconfig.json`, `shaders/*`

用中文來回答