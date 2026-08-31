#pragma once
#include <vector>
#include <ktx.h>
#include "Primitives.hpp"
#include "UUID.hpp"
#include "../render_api/Image.hpp"

namespace aZero::RenderAPI
{
	inline EImageFormat FromVK_Format(ktx_uint32_t format)
	{
		return (EImageFormat)format;
		//switch (format)
		//{
		//	// VK_FORMAT_R8G8B8A8_UNORM
		//case 37:
		//	return EImageFormat::RGBA8_UNORM;
		//	// VK_FORMAT_R8G8B8A8_SRGB 
		//case 43:
		//	return EImageFormat::RGBA8_UNORM_SRGB;
		//case 145:
		//	return EImageFormat::BC7_UNORM;
		//case 146:
		//	return EImageFormat::BC7_UNORM_SRGB;
		//default:
		//	return EImageFormat::UNKNOWN;
		//}
	}
}

namespace aZero::Asset
{
	struct AssetBase
	{
		std::string Name;
		std::array<char, aZero::Helper::UUID::GetSizeAsString()> UUID;
	};

	struct LegacyMeshData : public AssetBase
	{
		std::vector<aZero::Asset::Vertex> Vertices;
		std::vector<aZero::Asset::VertexIndex> Indices;
		std::vector<aZero::Asset::LegacySubMesh> Submeshes;
	};

	struct MeshletMeshData : public AssetBase
	{
		std::vector<aZero::Asset::Meshlet> Meshlets;
		std::vector<BoundingVolume::BoundingSphere> MeshletBounds;
		std::vector<aZero::Asset::Vertex> Vertices;
		std::vector<aZero::Asset::MeshletSubMesh> Submeshes;
	};

	struct ImageData : public AssetBase
	{
		struct Mip
		{
			std::uint32_t Offset;
			std::uint32_t RowPitch;
			std::uint32_t SlicePitch;
		};

		uint32_t BaseWidth;
		uint32_t BaseHeight;
		uint32_t FaceCount;
		uint32_t DepthOrArrayCount;
		std::vector<std::byte> Data;
		std::vector<Mip> MipLevels;
		aZero::RenderAPI::EImageFormat Format;
		aZero::RenderAPI::EImageType Type;
	};
}