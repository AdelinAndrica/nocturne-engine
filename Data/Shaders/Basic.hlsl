struct VSIn
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
};

struct VSOut
{
    float4 pos : SV_Position;
    float3 worldPos : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
};

struct LightData
{
    // positionType.xyz = position, w = 0 directional / 1 point / 2 spot
    float4 positionType;
    // directionRange.xyz = authored light forward direction, w = range
    float4 directionRange;
    // colorIntensity.rgb = linear light color, w = scalar intensity
    float4 colorIntensity;
    // spot.x = inner cone cosine, y = outer cone cosine
    float4 spot;
};

static const uint kMaxLights = 32;

// b0: per-frame
cbuffer PerFrame : register(b0)
{
    float4x4 gViewProj;
    LightData gLights[kMaxLights];
    uint gLightCount;
    float3 gPerFramePadding;
};

struct InstanceData
{
    float4x4 world;
    float4x4 normalWorld;
};

// t0: per-instance transforms
StructuredBuffer<InstanceData> gInstances : register(t0);

// b1: index of the first StructuredBuffer instance consumed by this draw.
// A root constant is used because StartInstanceLocation does not offset a
// manually indexed StructuredBuffer.
cbuffer PerDraw : register(b1)
{
    uint gInstanceBase;
};

VSOut VSMain(VSIn input, uint instanceId : SV_InstanceID)
{
    VSOut o;

    InstanceData instanceData =
        gInstances[gInstanceBase + instanceId];
    float4 worldPosition =
        mul(instanceData.world, float4(input.pos, 1.0));

    o.pos = mul(gViewProj, worldPosition);
    o.worldPos = worldPosition.xyz;
    o.worldNormal = normalize(
        mul((float3x3)instanceData.normalWorld, input.normal));
    return o;
}

float3 EvaluateLight(LightData light, float3 worldPos, float3 normal)
{
    const float type = light.positionType.w;
    const float3 color =
        max(light.colorIntensity.rgb, float3(0.0, 0.0, 0.0));
    const float intensity = max(light.colorIntensity.w, 0.0);

    float3 toLight = float3(0.0, 0.0, 0.0);
    float attenuation = 1.0;

    if (type < 0.5)
    {
        // Directional transform forward is the direction light travels.
        toLight = -normalize(light.directionRange.xyz);
    }
    else
    {
        const float3 delta = light.positionType.xyz - worldPos;
        const float distanceToLight = length(delta);
        if (distanceToLight <= 1.0e-5)
            return float3(0.0, 0.0, 0.0);

        toLight = delta / distanceToLight;

        // Design choice (not directly from the book): Phase 16 uses a smooth
        // finite-range falloff for the basic forward-lighting preview.
        const float range = max(light.directionRange.w, 1.0e-4);
        const float normalizedDistance =
            saturate(1.0 - distanceToLight / range);
        attenuation =
            normalizedDistance * normalizedDistance;

        if (type > 1.5)
        {
            const float3 lightToSurface = -toLight;
            const float coneCos = dot(
                normalize(light.directionRange.xyz),
                lightToSurface);
            const float outerCos = light.spot.y;
            const float innerCos = max(light.spot.x, outerCos + 1.0e-5);
            attenuation *= smoothstep(
                outerCos,
                innerCos,
                coneCos);
        }
    }

    const float ndotl =
        saturate(dot(normal, toLight));
    return color * (intensity * attenuation * ndotl);
}

float4 PSMain(VSOut input) : SV_Target0
{
    const float3 normal = normalize(input.worldNormal);

    // Design choice (not directly from the book): until the material/PBR phase,
    // the editor uses a neutral dielectric preview albedo plus low ambient fill.
    const float3 baseColor = float3(0.62, 0.68, 0.78);
    float3 irradiance = float3(0.16, 0.16, 0.18);

    [loop]
    for (uint i = 0; i < gLightCount; ++i)
        irradiance += EvaluateLight(gLights[i], input.worldPos, normal);

    return float4(baseColor * irradiance, 1.0);
}
