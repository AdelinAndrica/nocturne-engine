struct VSIn
{
    float3 pos : POSITION;
    float4 color : COLOR;
};

struct VSOut
{
    float4 pos : SV_Position;
    float4 color : COLOR;
};

cbuffer PerFrame : register(b0)
{
    float4x4 gViewProj;
};

VSOut VSMain(VSIn input)
{
    VSOut o;
    o.pos = mul(gViewProj, float4(input.pos, 1.0));
    o.color = input.color;
    return o;
}

float4 PSMain(VSOut input) : SV_Target0
{
    return input.color;
}
