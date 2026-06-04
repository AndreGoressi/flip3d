#pragma once

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
    float4 hdrParams;
};

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float wash = washParams.x;
    float4 color = float4(0.0f, 0.0f, 0.0f, wash);
    
    if (hdrParams.x > 0.0f)
    {
        float scaling = hdrParams.y / 80.0f;
        color.rgb *= scaling;
    }
    
    return color;
}
)";

inline constexpr const char *kCardVertexShader = R"(
cbuffer FrameCB : register(b0)
{
    row_major float4x4 viewProj;
    float4 washParams;
    float4 viewport;
    float4 hdrParams;
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
    float4 hdrParams;
};

float3 SRGBToLinear(float3 c)
{
    return c < 0.04045f ? c / 12.92f : pow(c * (1.0f / 1.055f) + 0.055f / 1.055f, 2.4f);
}

float3 LinearToST2084(float3 linearRGB)
{
    float m1 = 2610.0f / 16384.0f;
    float m2 = 2523.0f / 32.0f;
    float o1 = 3424.0f / 4096.0f;
    float o2 = 2413.0f / 4096.0f;

    float3 L = pow(max(0.0f, linearRGB), m1);
    float3 num = o1 + o2 * L;
    float3 den = 1.0f + 32.0f * L;
    return pow(num / den, m2);
}

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0, float4 color : COLOR0, float4 accent : COLOR1) : SV_TARGET
{
    float4 windowColor = cardTexture.Sample(cardSampler, uv);
    float3 linearRGB = SRGBToLinear(windowColor.rgb);
    
    linearRGB *= washParams.w;
    
    float alpha = windowColor.a * color.a;

    if (hdrParams.x > 0.0f)
    {
        float nits = hdrParams.y; 
        float3 normalizedHDR = (linearRGB * nits) / 10000.0f;
        normalizedHDR *= alpha; 
        
        float3 finalPQ = LinearToST2084(normalizedHDR);
        return float4(finalPQ, alpha);
    }
    else
    {
        linearRGB = pow(abs(linearRGB), 1.0f / 2.2f);
        return float4(linearRGB * alpha, alpha);
    }
}
)";
