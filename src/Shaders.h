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

inline constexpr const char* kFXAAPixelShader = R"(
Texture2D texInput : register(t0);
SamplerState samLinear : register(s0);

cbuffer FXAAConstants : register(b2) 
{
    float2 rcpFrame;
    float fxaaSpanMax;
    float fxaaReduceMul;
    float fxaaReduceMin;
};

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float originalAlpha = texInput.Sample(samLinear, uv).a;

    float3 rgbNW = texInput.Sample(samLinear, uv + float2(-rcpFrame.x, -rcpFrame.y)).rgb;
    float3 rgbNE = texInput.Sample(samLinear, uv + float2( rcpFrame.x, -rcpFrame.y)).rgb;
    float3 rgbSW = texInput.Sample(samLinear, uv + float2(-rcpFrame.x,  rcpFrame.y)).rgb;
    float3 rgbSE = texInput.Sample(samLinear, uv + float2( rcpFrame.x,  rcpFrame.y)).rgb;
    float3 rgbM  = texInput.Sample(samLinear, uv).rgb;

    float3 luma = float3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    float2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * fxaaReduceMul * 0.25, fxaaReduceMin);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

    dir = saturate(dir * rcpDirMin) * fxaaSpanMax * rcpFrame;

    float3 rgbA = 0.5 * (
        texInput.Sample(samLinear, uv + dir * (1.0/3.0 - 0.5)).rgb +
        texInput.Sample(samLinear, uv + dir * (2.0/3.0 - 0.5)).rgb
    );

    float3 rgbB = rgbA * 0.5 + 0.25 * (
        texInput.Sample(samLinear, uv + dir * -0.5).rgb +
        texInput.Sample(samLinear, uv + dir * 0.5).rgb
    );

    float lumaB = dot(rgbB, luma);
    
    float3 finalRGB = (lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB;
    return float4(finalRGB, originalAlpha); 
}
)";
