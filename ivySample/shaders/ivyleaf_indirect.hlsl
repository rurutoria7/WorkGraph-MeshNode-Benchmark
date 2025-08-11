// Simple leaf rendering shader for ExecuteIndirect
// This file is part of the AMD Work Graph Ivy Generation Sample.

// Vertex input
struct VSInput
{
    float3 Position : POSITION;
};

// Vertex output / Pixel input  
struct PSInput
{
    float4 Position : SV_POSITION;
};

// Vertex Shader
PSInput VSMain(VSInput input)
{
    PSInput output;
    
    // Simple dummy transformation to clip space
    // Place geometry at a reasonable distance from camera and apply basic perspective
    float3 worldPos = input.Position * 0.1f; // Scale down first
    
    // Apply a simple translation to position geometry in front of camera
    worldPos.z += 5.0f; // Move 5 units away from camera (assuming camera at origin looking down -Z)
    
    // Simple perspective projection (dummy matrices)
    // Assume camera FOV ~60 degrees, aspect ratio 16:9, near=0.1, far=100
    float4 clipPos;
    clipPos.x = worldPos.x * 1.0f; // Simple X scaling
    clipPos.y = worldPos.y * 1.0f; // Simple Y scaling  
    clipPos.z = (worldPos.z - 0.1f) / (100.0f - 0.1f); // Normalize Z to [0,1]
    clipPos.w = worldPos.z; // Perspective divide by Z
    
    // Apply perspective divide manually for now
    clipPos.xyz /= clipPos.w;
    clipPos.w = 1.0f;
    
    output.Position = clipPos;
    
    return output;
}

// Pixel Shader - output red color for testing
float4 PSMain(PSInput input) : SV_TARGET
{
    return float4(1.0f, 0.0f, 0.0f, 1.0f); // Red color
}
