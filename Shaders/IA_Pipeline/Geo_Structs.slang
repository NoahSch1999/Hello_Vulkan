#ifndef TEMP_DEFINITIONS_INCLUDED
#define TEMP_DEFINITIONS_INCLUDED
#include "../Volumes.slang"
#include "../Util.slang"

#define TREADS_PER_WAVE 32

struct Payload
{
    uint MeshletIndex[32];
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
    float3 Meshletid : meshletid;
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

// GPU_Drive_Structs.hpp MIRROR

struct CameraData
{
    float4x4 ViewMatrix;
    float4x4 ViewProjectionMatrix;
    BoundingFrustum Frustum;
};

struct IndirectArgumentCounter
{
    uint32_t Count;
};

struct InstanceData
{
    float4x4 Transform;
};

struct IndirectArgumentConstantData
{
    float4x4 Instance;
    uint32_t GlobalMeshletOffset;
    uint32_t GlobalVertexOffset;
    uint32_t MaterialIndex;
    uint32_t MeshletCount;
};

struct ObjectCullData
{
    BoundingSphere Bounds;
    uint32_t InstanceDataIndex;
    uint32_t GlobalMeshletOffset;
    uint32_t GlobalVertexOffset;
    uint32_t MeshletCount;
    uint32_t MaterialIndex;
};

struct MeshCullConstantsData
{
    uint32_t MeshInstanceCount;
    uint3 Pad;
};

struct IndirectArguments
{
    IndirectArgumentConstantData Data;
    uint32_t GroupX;
    uint32_t GroupY;
    uint32_t GroupZ;
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

struct PushConstants
{
    // CS CULL => AS
    IndirectArgumentConstantData IA_Args;

    // AS, MS, PS
    uint64_t CameraAddress;

    // AS
    uint64_t MeshletBoundBufferAddress;

    // MS
    uint64_t MeshletBufferAddress;
    uint64_t VertexBufferAddress;

    uint64_t MaterialBufferAddress;

    uint32_t RenderMode;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> Constants;

#endif