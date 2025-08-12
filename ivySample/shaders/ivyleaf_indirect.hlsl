// Simple leaf rendering shader for ExecuteIndirect
// This file is part of the AMD Work Graph Ivy Generation Sample.

#include "ivycommon.h"

// Vertex input
struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
};

// Vertex output / Pixel input  
struct PSInput
{
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL;
};

// Vertex Shader
PSInput VSMain(VSInput input)
{
    PSInput output;
    
    float4 worldSpacePosition = float4(input.Position * 10.0f, 1.0f);
    worldSpacePosition.y += 3;
    
    output.Position = mul(ViewProjection, worldSpacePosition);
    
    output.Normal = normalize(input.Normal);
    
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 normalColor = input.Normal * 0.5f + 0.5f;
    return float4(normalColor, 1.0f); // Normal as RGB color
}
