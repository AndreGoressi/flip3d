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
    float4 windowColor = cardTexture.Sample(cardSampler, uv);
    float alpha = windowColor.a * color.a;
    float3 lit = windowColor.rgb * washParams.w;
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
    float4 viewport; 
};

float RGBToLuma(float3 rgb)
{
    return dot(rgb, float3(0.299f, 0.587f, 0.114f));
}

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float2 texelSize = 1.0f / viewport.xy;
    float4 e = sceneTexture.Sample(sceneSampler, uv); // Zentrum
    float4 b = sceneTexture.Sample(sceneSampler, uv + float2(0.0f, -texelSize.y));
    float4 d = sceneTexture.Sample(sceneSampler, uv + float2(-texelSize.x, 0.0f));
    float4 f = sceneTexture.Sample(sceneSampler, uv + float2(texelSize.x, 0.0f));
    float4 h = sceneTexture.Sample(sceneSampler, uv + float2(0.0f, texelSize.y));
    float lumaB = RGBToLuma(b.rgb);
    float lumaD = RGBToLuma(d.rgb);
    float lumaE = RGBToLuma(e.rgb);
    float lumaF = RGBToLuma(f.rgb);
    float lumaH = RGBToLuma(h.rgb);
    float mn = min(lumaE, min(min(lumaB, lumaD), min(lumaF, lumaH)));
    float mx = max(lumaE, max(max(lumaB, lumaD), max(lumaF, lumaH)));

    float amg = mx - mn;
    if (amg < 0.0001f) return e;

    float4 edgeAA = (b + d + f + h + e) * 0.2f;

    float sharpness = -0.125f; 
    float w = amg * sharpness;
    
    float rcpWeight = 1.0f / (1.0f + 4.0f * w);
    float4 casSharp = saturate((b * w + d * w + f * w + h * w + e) * rcpWeight);

    float edgeThreshold = saturate(amg * 4.0f); 
    
    return lerp(casSharp, edgeAA, edgeThreshold * 0.35f);
}
)";
