#pragma once

// ============================================================================
// HLSL shader source code strings
// ============================================================================

Texture2D<float4> cardTexture : register(t0);
SamplerState cardSampler : register(s0);

cbuffer FrameCB : register(b0)
{
    row_major float4x4 viewProj;
    float4 washParams;
    float4 viewport;
};

float4 main(float4 position : SV_POSITION,
            float2 uv : TEXCOORD0,
            float4 color : COLOR0,
            float4 accent : COLOR1) : SV_TARGET
{
    float4 windowColor = cardTexture.Sample(cardSampler, uv);
    float alpha = windowColor.a * color.a;
    return float4(windowColor.rgb, alpha);
}

