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

// b0: per-frame
cbuffer PerFrame : register(b0)
{
    float4x4 gViewProj;
};

// t0: per-instance world matrices
StructuredBuffer<float4x4> gWorld : register(t0);

VSOut VSMain(VSIn input, uint instanceId : SV_InstanceID)
{
    VSOut o;

    float4 wpos = mul(gWorld[instanceId], float4(input.pos, 1.0));
    o.pos = mul(gViewProj, wpos);
    o.color = input.color;
    return o;
}

float4 PSMain(VSOut input) : SV_Target0
{
    return input.color;
}
