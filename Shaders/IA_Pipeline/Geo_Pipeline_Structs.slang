#include "../Volumes.slang"
#include "../Util.slang"

struct CameraData
{
    float4x4 ViewMatrix;
    float4x4 ViewProjectionMatrix;
    BoundingFrustum Frustum;
};

struct InstanceData
{
    float4x4 WorldMatrix;
    uint64_t MeshletBDA;
    uint64_t VertexBDA;
    uint32_t MeshletCount;
    uint32_t MaterialIndex;
};

struct InstanceCullData
{
    BoundingSphere WorldSpaceBounds;
    uint32_t MeshletCount;
};

struct GeoDrawPushConstants
{
    uint64_t CameraBufferPtr;
    uint64_t MeshletBoundBufferPtr;
    uint64_t MaterialBufferPtr;
    uint64_t InstanceDataBufferPtr;
    uint64_t DrawMetadataBufferPtr;

    uint32_t RenderMode;
};

struct InstanceCullPushConstants
{
    uint64_t IndirectArgumentBufferPtr;
    uint64_t CameraBufferPtr;
    uint64_t InstanceCullDataBufferPtr;
    uint64_t DrawMetadataBufferPtr;
};

struct MeshTaskIndirectArgs
{
    uint32_t GroupX;
    uint32_t GroupY;
    uint32_t GroupZ;
};

struct DrawMetaData
{
    uint32_t InstanceIndex;
    uint32_t MeshletCount;
};

#define THREAD_COUNT_X 32