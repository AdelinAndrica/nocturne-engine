struct VSOut
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

VSOut VSMain(uint vertexId : SV_VertexID)
{
    float2 clip;
    if (vertexId == 0)
        clip = float2(-1.0, -1.0);
    else if (vertexId == 1)
        clip = float2(-1.0,  3.0);
    else
        clip = float2( 3.0, -1.0);

    VSOut o;
    o.pos = float4(clip, 0.0, 1.0);
    o.uv = float2(clip.x * 0.5 + 0.5, 0.5 - clip.y * 0.5);
    return o;
}

float4 PSMain(VSOut input) : SV_Target0
{
    // Design choice (not directly from the book): restrained editor-sky colors
    // provide depth/spatial context without pulling lighting/PBR into Phase 14.
    const float3 zenith = float3(0.018, 0.038, 0.075);
    const float3 upperHorizon = float3(0.085, 0.130, 0.205);
    const float3 lowerHorizon = float3(0.035, 0.045, 0.065);

    float y = saturate(input.uv.y);
    float upper = smoothstep(0.0, 0.70, y);
    float3 color = lerp(zenith, upperHorizon, upper);

    float lower = smoothstep(0.70, 1.0, y);
    color = lerp(color, lowerHorizon, lower);

    return float4(color, 1.0);
}
