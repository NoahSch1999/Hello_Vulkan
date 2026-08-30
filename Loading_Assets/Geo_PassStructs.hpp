#pragma once
#include "Primitives.hpp"

namespace aZero::Rendering
{
    struct CameraData
    {
        glm::mat4 ViewMatrix;
        glm::mat4 ViewProjectionMatrix;
        aZero::BoundingVolume::BoundingFrustum Frustum;
    };

    struct InstanceData
    {
        glm::mat4 WorldMatrix;
        VkDeviceAddress MeshletBDA;
        VkDeviceAddress VertexBDA;
        uint32_t MeshletCount;
        uint32_t MaterialIndex;
    };

    struct InstanceCullData
    {
        aZero::BoundingVolume::BoundingSphere WorldSpaceBounds;
        uint32_t MeshletCount;
    };

    struct GeoDrawPushConstants
    {
        VkDeviceAddress CameraBufferPtr;
        VkDeviceAddress MeshletBoundBufferPtr;
        VkDeviceAddress MaterialBufferPtr;
        VkDeviceAddress InstanceDataBufferPtr;
        VkDeviceAddress DrawMetadataBufferPtr;

        uint32_t RenderMode;
    };

    struct InstanceCullPushConstants
    {
        VkDeviceAddress IndirectArgumentBufferPtr;
        VkDeviceAddress CameraBufferPtr;
        VkDeviceAddress InstanceCullDataBufferPtr;
        VkDeviceAddress DrawMetadataBufferPtr;
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

    struct PBRMaterial
    {
        // todo Use the albedo alpha channel for roughness and normal w channel for metallic
        uint32_t AlbedoTexture;
        uint32_t NormalMap;
        uint32_t RoughnessMetallic;
        uint32_t GlowMap;
        uint32_t TransparencyMap;
    };
}