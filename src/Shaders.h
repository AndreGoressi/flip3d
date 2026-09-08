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

// Separable Gaussian blur, one direction per pass (horizontal then vertical).
// Shares kBackgroundVertexShader's full-screen triangle.
inline constexpr const char *kBlurPixelShader = R"(
Texture2D<float4> srcTexture : register(t0);
SamplerState srcSampler : register(s0);

cbuffer BlurCB : register(b0)
{
    float4 texelSizeAndDirection; // xy = 1/width, 1/height; zw = direction
};

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    static const float weights[5] = { 0.227027f, 0.1945946f, 0.1216216f, 0.054054f, 0.016216f };
    const float2 texel = texelSizeAndDirection.xy;
    const float2 dir = texelSizeAndDirection.zw;

    float3 result = srcTexture.Sample(srcSampler, uv).rgb * weights[0];
    [unroll]
    for (int i = 1; i < 5; ++i)
    {
        // Tap spacing of 2 texels between samples widens the effective blur
        // radius per pass without needing more samples.
        const float2 offset = dir * texel * (float)i * 2.0f;
        result += srcTexture.Sample(srcSampler, uv + offset).rgb * weights[i];
        result += srcTexture.Sample(srcSampler, uv - offset).rgb * weights[i];
    }
    return float4(result, 1.0f);
}
)";

// Replaces kBackgroundPixelShader's flat black wash: samples the blurred
// desktop capture and dims it toward black. ALWAYS outputs alpha = 1 —
// that full opacity is what actually stops real windows from showing
// through (see: DrawAcrylic removal). Native blur-behind deliberately did
// the opposite (let the real desktop bleed through, blurred); this doesn't,
// because it's our own fully opaque render of an already-captured image,
// not a live window into whatever's really behind our own window.
inline constexpr const char *kDesktopWashPixelShader = R"(
Texture2D<float4> desktopTexture : register(t0);
SamplerState desktopSampler : register(s0);

cbuffer FrameCB : register(b0)
{
    row_major float4x4 viewProj;
    float4 washParams;
    float4 viewport;
};

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float3 desktop = desktopTexture.Sample(desktopSampler, uv).rgb;
    float dim = saturate(washParams.x);
    float3 result = lerp(desktop, float3(0.0f, 0.0f, 0.0f), dim * 0.55f);
    return float4(result, 1.0f);
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
    float4 flags;
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
    float4 flags : COLOR2;
};

VSOut main(VSIn input)
{
    VSOut output;
    float4 worldPosition = mul(float4(input.position, 1.0f), world);
    output.position = mul(worldPosition, viewProj);
    output.uv = input.uv;
    output.color = color;
    output.accent = accent;
    output.flags = flags;
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
}

cbuffer ObjectCB : register(b1)
{
    row_major float4x4 world;
    float4 color;
    float4 accent;
    float4 flags;
}

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0, float4 colorIn : COLOR0, float4 accentIn : COLOR1, float4 flagsIn : COLOR2) : SV_TARGET
{
    float4 windowColor = cardTexture.Sample(cardSampler, uv);

    float3 rgb = windowColor.rgb * 0.8f;

    rgb = pow(max(rgb, 0.0001f), 0.4545f);

    if (flagsIn.x > 0.5f)
    {
        rgb *= 0.7f;
    }
    else
    {
        rgb *= 1.0f;
    }

    uint width = 0;
    uint height = 0;
    cardTexture.GetDimensions(width, height);
    float2 texSize = float2(width, height);

    float radius = (accentIn.x >= 1.0f && accentIn.x <= 128.0f) ? accentIn.x : 16.0f;
    float2 p = (uv - 0.5f) * texSize;
    float2 halfSize = texSize * 0.5f;
    float2 q = abs(p) - halfSize + radius;
    float sdfTex = min(max(q.x, q.y), 0.0f) + length(max(q, 0.0f)) - radius;

    float sdfScreen = sdfTex / max(fwidth(sdfTex), 0.00001f);
    float edgeAlpha = saturate(0.5f - sdfScreen);

    float alpha = windowColor.a * colorIn.a * edgeAlpha;
    float3 lit = rgb * washParams.w;

    return float4(lit * alpha, alpha);
}
)";
