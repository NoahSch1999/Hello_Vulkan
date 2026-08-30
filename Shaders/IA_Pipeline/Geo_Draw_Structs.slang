#ifndef TEMP_DEFINITIONS_INCLUDED
#define TEMP_DEFINITIONS_INCLUDED
#include "Geo_Pipeline_Structs.slang"

struct Payload
{
    uint MeshletIndex[32];
    uint InstanceIndex[32];
    uint MeshletCount[32];
};

// MESH ASSET STRUCTS

#define g_VerticesPerMeshlet 64
#define g_PrimitivesPerMeshlet 84

struct RasterVertex
{
    float4 Position : SV_Position;
    float3 WorldPosition : WORLD_POSITION;
    float2 UV : UV;
    float3 Normal : NORMAL;
    float3 Meshletid : MESHLET_ID;
    nointerpolation uint32_t MaterialIndex : MATERIAL_INDEX;
};

struct MeshVertex
{
    float3 Position;
    uint32_t UV;
    uint32_t Normal;
};

struct Meshlet
{
    uint32_t VertexOffset;
    uint32_t VertexCount;
    uint32_t PrimitiveCount;
    uint32_t Primitives[g_PrimitivesPerMeshlet];
};

struct PhongPixelConstantData
{
    uint32_t SamplerIndex;
    uint32_t MaterialBuffer;
    uint32_t PointLightBuffer;
    uint32_t SpotLightBuffer;
    uint32_t DirectionalLightBuffer;
    float Time;
};

//

#define RENDER_MODE_LIT 0
#define RENDER_MODE_UNLIT 1
#define RENDER_MODE_NORMAL_MAP 2
#define RENDER_MODE_GEOMETRY_NORMALS 3
#define RENDER_MODE_NORMAL_MAP_NORMALS 4
#define RENDER_MODE_MESHLET_IDS 5
#define RENDER_MODE_UVS 6
#define RENDER_MODE_MIP_LEVEL 7

[[vk::push_constant]]
ConstantBuffer<GeoDrawPushConstants> Constants;

#endif