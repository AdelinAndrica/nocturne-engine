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
    float gTime;
    float3 _pad;
};

VSOut VSMain(VSIn v)
{
    VSOut o;
    o.pos = float4(v.pos, 1.0);
    o.color = v.color;
    return o;
}

float4 PSMain(VSOut i) : SV_Target
{
    return i.color;
}
