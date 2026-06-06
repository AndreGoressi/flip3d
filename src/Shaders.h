#pragma once

// ============================================================================
// HLSL shader source code strings
// ============================================================================

float3 DecodeSDR(float3 c)
{
    return pow(c, 2.2);
}
float3 EncodeSDR(float3 c)
{
    return pow(c, 1.0 / 2.2);
}

float3 DecodePQ(float3 c)
{
    // PQ → linear Nits
    const float m1 = 2610.0 / 16384.0;
    const float m2 = 2523.0 / 32.0;
    const float c1 = 3424.0 / 4096.0;
    const float c2 = 2413.0 / 128.0;
    const float c3 = 2392.0 / 128.0;

    float3 cp = pow(c, 1.0 / m2);
    return pow(max(cp - c1, 0.0) / (c2 - c3 * cp), 1.0 / m1);
}
float3 EncodePQ(float3 L)
{
    const float m1 = 2610.0 / 16384.0;
    const float m2 = 2523.0 / 32.0;
    const float c1 = 3424.0 / 4096.0;
    const float c2 = 2413.0 / 128.0;
    const float c3 = 2392.0 / 128.0;

    float3 Lm1 = pow(L, m1);
    return pow((c1 + c2 * Lm1) / (1.0 + c3 * Lm1), m2);
}


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
    //float alpha = windowColor.a * color.a;
    //float3 lit = windowColor.rgb * washParams.w;  // ambient light
    //return float4(lit * alpha, alpha);
    float3 linear;
    //
    if (isHDR)
        linear = DecodePQ(windowColor.rgb);
    else
        linear = DecodeSDR(windowColor.rgb);

    linear *= washParams.w;

    float3 encoded;
    if (isHDR)
        encoded = EncodePQ(linear);
    else
        encoded = EncodeSDR(linear);

    return float4(encoded, windowColor.a * color.a);
    }
)";
