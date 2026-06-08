#pragma once

// ============================================================================
// HLSL shader source code strings
// ============================================================================

inline constexpr const char *kBackgroundVertexShader = R"(
struct VSOut
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOut main(uint vertexId : SV_VertexID)
{
    float2 pos;
    pos.x = (vertexId == 2) ? 3.0f : -1.0f;
    pos.y = (vertexId == 1) ? 3.0f : -1.0f;

    VSOut output;
    output.position = float4(pos, 0.0f, 1.0f);
    output.uv = float2(0.5f * (pos.x + 1.0f), 1.0f - (0.5f * (pos.y + 1.0f)));
    return output;
}
)";

inline constexpr const char *kBackgroundPixelShader = R"(
cbuffer FrameCB : register(b0)
{
    row_major float4x4 viewProj;
    float4 washParams;
    float4 viewport;
};

// Simplified wash matching uDWM: solid black with alpha = enterProgress * 0.5
float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float wash = washParams.x;
    return float4(0.0f, 0.0f, 0.0f, wash);
}
)";

inline constexpr const char *kCardVertexShader = R"(
cbuffer FrameCB : register(b0)
{
    row_major float4x4 viewProj;
    float4 washParams;
    float4 viewport;
};

cbuffer ObjectCB : register(b1)
{
    row_major float4x4 world;
    float4 color;
    float4 accent;
};

struct VSIn
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VSOut
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float4 accent : COLOR1;
};

VSOut main(VSIn input)
{
    VSOut output;
    float4 worldPosition = mul(float4(input.position, 1.0f), world);
    output.position = mul(worldPosition, viewProj);
    output.uv = input.uv;
    output.color = color;
    output.accent = accent;
    return output;
}
)";

inline constexpr const char *kCardPixelShader = R"(
Texture2D<float4> cardTexture : register(t0);
SamplerState cardSampler : register(s0);

cbuffer FrameCB : register(b0)
{
    row_major float4x4 viewProj;
    float4 washParams;
    float4 viewport;
};

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0, float4 color : COLOR0, float4 accent : COLOR1) : SV_TARGET
{
    // Sample captured window content; premultiply for DXGI_ALPHA_MODE_PREMULTIPLIED.
    float4 windowColor = cardTexture.Sample(cardSampler, uv);
    float alpha = windowColor.a * color.a;
    float3 lit = windowColor.rgb * washParams.w;  // ambient light
    return float4(lit * alpha, alpha);
}
)"; 

inline constexpr const char *kPostProcessPixelShader = R"(
Texture2D<float4> sceneTexture : register(t0);
SamplerState sceneSampler : register(s0);

cbuffer FrameCB : register(b0)
{
    row_major float4x4 viewProj;
    float4 washParams;
    float4 viewport; // x = width, y = height
};

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float2 texelSize = 1.0f / viewport.xy;
    float3 luma = float3(0.299f, 0.587f, 0.114f);

    float4 colorCenter = sceneTexture.Sample(sceneSampler, uv);
    float lumaCenter = dot(colorCenter.rgb, luma);
    
    float lumaDown   = dot(sceneTexture.Sample(sceneSampler, uv + float2(0.0f, texelSize.y)).rgb, luma);
    float lumaUp     = dot(sceneTexture.Sample(sceneSampler, uv - float2(0.0f, texelSize.y)).rgb, luma);
    float lumaLeft   = dot(sceneTexture.Sample(sceneSampler, uv - float2(texelSize.x, 0.0f)).rgb, luma);
    float lumaRight  = dot(sceneTexture.Sample(sceneSampler, uv + float2(texelSize.x, 0.0f)).rgb, luma);

    float lumaMin = min(lumaCenter, min(min(lumaDown, lumaUp), min(lumaLeft, lumaRight)));
    float lumaMax = max(lumaCenter, max(max(lumaDown, lumaUp), max(lumaLeft, lumaRight)));
    float lumaRange = lumaMax - lumaMin;

    if (lumaRange < max(0.04f, lumaMax * 0.125f))
    {
        return colorCenter;
    }

    float lumaLeftDown  = dot(sceneTexture.Sample(sceneSampler, uv + float2(-texelSize.x,  texelSize.y)).rgb, luma);
    float lumaRightUp   = dot(sceneTexture.Sample(sceneSampler, uv + float2( texelSize.x, -texelSize.y)).rgb, luma);
    float lumaLeftUp    = dot(sceneTexture.Sample(sceneSampler, uv + float2(-texelSize.x, -texelSize.y)).rgb, luma);
    float lumaRightDown = dot(sceneTexture.Sample(sceneSampler, uv + float2( texelSize.x,  texelSize.y)).rgb, luma);

    float dirX = -((lumaLeftUp + lumaLeftDown) - (lumaRightUp + lumaRightDown));
    float dirY = ((lumaLeftUp + lumaRightUp) - (lumaLeftDown + lumaRightDown));

    float dirReduce = max((lumaLeftUp + lumaRightUp + lumaLeftDown + lumaRightDown) * 0.25f * 0.125f, 0.00001f);
    float rcpDirMin = 1.0f / (min(abs(dirX), abs(dirY)) + dirReduce);

    float2 dir = min(float2(6.0f, 6.0f), max(float2(-6.0f, -6.0f), float2(dirX, dirY) * rcpDirMin)) * texelSize;

    float4 rgbA = 0.5f * (
        sceneTexture.Sample(sceneSampler, uv + dir * (1.0f / 3.0f - 0.5f)) +
        sceneTexture.Sample(sceneSampler, uv + dir * (2.0f / 3.0f - 0.5f)));
    float4 rgbB = rgbA * 0.5f + 0.25f * (
        sceneTexture.Sample(sceneSampler, uv + dir * -0.5f) +
        sceneTexture.Sample(sceneSampler, uv + dir * 0.5f));

    float lumaB = dot(rgbB.rgb, luma);
    float blendWeight = saturate((lumaRange - 0.05f) / 0.2f);
    
    float lumaAvg = (lumaUp + lumaDown + lumaLeft + lumaRight) * 0.25f;
    float subpixelDiff = abs(lumaAvg - lumaCenter);
    float subpixelWeight = saturate(subpixelDiff / lumaRange);
    blendWeight *= (1.0f - subpixelWeight * 0.6f); 

    if ((lumaB < lumaMin) || (lumaB > lumaMax))
    {
        return lerp(colorCenter, rgbA, blendWeight);
    }
    return lerp(colorCenter, rgbB, blendWeight);
}
)";
