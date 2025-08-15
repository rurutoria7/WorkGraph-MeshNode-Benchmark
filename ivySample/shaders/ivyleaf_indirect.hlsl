// Simple leaf rendering shader for ExecuteIndirect
// This file is part of the AMD Work Graph Ivy Generation Sample.

#include "ivycommon.h"

// Instance data buffer
StructuredBuffer<IvyInstanceData> g_instance_data : register(t0);

// Vertex input
struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    uint InstanceID : SV_InstanceID;
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
    
    // Get instance transform from structured buffer
    float4x4 instanceTransform = g_instance_data[input.InstanceID].transform;
    
    // Apply instance transform to vertex position
    float4 localPosition = float4(input.Position, 1.0f);
    float4 worldSpacePosition = mul(instanceTransform, localPosition);
    
    output.Position = mul(ViewProjection, worldSpacePosition);
    
    // Transform normal by instance transform (3x3 part)
    output.Normal = normalize(mul((float3x3)instanceTransform, input.Normal));
    
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 normalColor = input.Normal * 0.5f + 0.5f;
    return float4(normalColor, 1.0f); // Normal as RGB color
}
