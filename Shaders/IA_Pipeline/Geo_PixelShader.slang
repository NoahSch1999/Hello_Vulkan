#include "Geo_Draw_Structs.slang"

struct Output
{
    float4 color : SV_TARGET0;
};

struct PBRMaterial
{
    // todo Use the albedo alpha channel for roughness and normal w channel for metallic
    uint AlbedoTexture;
    uint NormalMap;
    uint RoughnessMetallic;
    uint GlowMap;
    uint TransparencyMap;
};

[[vk::binding(0, 0)]] // [vk::binding(binding : int, set : int)]
Sampler2D Textures[];

[shader("fragment")]
Output psMain(RasterVertex pin)
{
    Output output;

    PBRMaterial *materialBuffer = (PBRMaterial *)Constants.MaterialBufferPtr;

    PBRMaterial material = materialBuffer[pin.MaterialIndex];

    if (material.AlbedoTexture == 0xffffffff)
    {
        output.color = float4(1, 0, 1, 1);
        return output;
    }

    float3 sampledAlbedo = Textures[material.AlbedoTexture].Sample(pin.UV).xyz;

    float3 fragmentNormal;
    if (material.NormalMap == 0xffffffff)
    {
        fragmentNormal = normalize(pin.Normal);
    }
    else
    {
        const float3 sampledNormal = Textures[material.NormalMap].Sample(pin.UV).xyz;
        float3 tangent;
        float3 bitangent;
        float3 normal = normalize(pin.Normal);
        CalcTangentAndBitangent(normal, tangent, bitangent);
        float3x3 TBN = float3x3(tangent, bitangent, normal);
        fragmentNormal = normalize(sampledNormal * 2.f - 1.f);
        fragmentNormal = normalize(mul(fragmentNormal, TBN));
    }

    if (Constants.RenderMode != RENDER_MODE_LIT)
    {
        // -------------------------------   DEBUG ALBEDO   -------------------------------------------
        if (Constants.RenderMode == RENDER_MODE_UNLIT)
        {
            output.color = float4(sampledAlbedo, 1);
            return output;
        }
        // --------------------------------------------------------------------------------------------

        if (material.NormalMap != 0xffffffff)
        {
            // -------------------------------   DEBUG NORMAL MAP   -----------------------------------
            if (Constants.RenderMode == RENDER_MODE_NORMAL_MAP)
            {
                const float3 sampledNormal = Textures[material.NormalMap].Sample(pin.UV).xyz;
                output.color = float4(sampledNormal * 0.5 + 0.5, 1);
                return output;
            }
            // ----------------------------------------------------------------------------------------

            // -------------------------------   DEBUG NORMALS WITH MAPS   ----------------------------
            if (Constants.RenderMode == RENDER_MODE_NORMAL_MAP_NORMALS)
            {
                output.color = float4(fragmentNormal * 0.5 + 0.5, 1);
                return output;
            }
            // ----------------------------------------------------------------------------------------
        }

        // -------------------------------   DEBUG UV   -----------------------------------------------
        if (Constants.RenderMode == RENDER_MODE_UVS)
        {
            output.color = float4(pin.UV, 0, 1);
            return output;
        }
        // --------------------------------------------------------------------------------------------

        // -------------------------------   DEBUG NORMALS  -------------------------------------------
        if (Constants.RenderMode == RENDER_MODE_GEOMETRY_NORMALS)
        {
            output.color = float4(pin.Normal * 0.5 + 0.5, 1);
            return output;
        }
        // --------------------------------------------------------------------------------------------

        // -------------------------------   DEBUG MESHLETS   -----------------------------------------
        if (Constants.RenderMode == RENDER_MODE_MESHLET_IDS)
        {
            output.color = float4(pin.Meshletid, 1);
            return output;
        }
        // --------------------------------------------------------------------------------------------

        // -------------------------------   DEBUG MIP LEVEL   ----------------------------------------
        if (Constants.RenderMode == RENDER_MODE_MIP_LEVEL)
        {
            float lod = Textures[material.AlbedoTexture].CalculateLevelOfDetail(pin.UV);
            float t = saturate(lod / 13.f);
            output.color = float4(1.f - t, 1.f - t, 1.f - t, 1.f);
            return output;
        }
        // --------------------------------------------------------------------------------------------
    }

    // todo Normal pass calculations
    output.color = float4(sampledAlbedo, 1.f);
    return output;
}