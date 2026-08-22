#pragma once
#include <filesystem>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

#include "AssetData.hpp"
#include "ImageLoading.hpp"

namespace aZero::File
{
	inline aZero::Asset::LegacyMeshData ExtractGltfLegacyMesh(const fastgltf::Mesh& mesh, fastgltf::Asset* asset)
	{
		const uint32_t numPrimitives = std::clamp((uint32_t)mesh.primitives.size(), 0u, aZero::Asset::g_MaxNumSubmeshes); // Currently only supports atmost 10 submeshes
		aZero::Asset::LegacyMeshData meshData;
		meshData.Name = mesh.name.c_str();
		meshData.Submeshes.resize(numPrimitives);

		uint32_t baseIndex = 0;
		for (int i = 0; i < numPrimitives; i++)
		{
			const auto& primitive = mesh.primitives[i];
			size_t baseColorTexcoordIndex = 0;

			if (primitive.materialIndex.has_value())
			{
				const fastgltf::Material& material = asset->materials[primitive.materialIndex.value()];
				if (material.pbrData.baseColorTexture.has_value())
				{
					if (material.pbrData.baseColorTexture->transform && material.pbrData.baseColorTexture->transform->texCoordIndex.has_value()) {
						baseColorTexcoordIndex = material.pbrData.baseColorTexture->transform->texCoordIndex.value();
					}
					else {
						baseColorTexcoordIndex = material.pbrData.baseColorTexture->texCoordIndex;
					}
				}
			}

			auto& positionAccessor = asset->accessors[primitive.findAttribute("POSITION")->accessorIndex];
			/*if (!positionAccessor.bufferViewIndex.has_value())
				continue;*/

			std::vector<Asset::Vertex> vertexData(positionAccessor.count);
			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(*asset, positionAccessor, [&vertexData](fastgltf::math::fvec3 pos, std::size_t idx) {
				auto position = fastgltf::math::fvec3(pos.x(), pos.y(), pos.z());
				vertexData[idx].Position = { position.x(), position.y(), position.z() };
				});

			auto& normalAccessor = asset->accessors[primitive.findAttribute("NORMAL")->accessorIndex];
			/*if (!normalAccessor.bufferViewIndex.has_value())
				continue;*/

			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(*asset, normalAccessor, [&vertexData](fastgltf::math::fvec3 n, std::size_t idx) {
				vertexData[idx].Normal = Asset::PackNormal(Asset::EncodeNormalOctahedral({ n.x(), n.y(), n.z() }));
				});

			const std::string texcoordAttribute = std::string("TEXCOORD_") + std::to_string(baseColorTexcoordIndex);
			if (const auto* texcoord = primitive.findAttribute(texcoordAttribute); texcoord != primitive.attributes.end()) {
				// Tex coord
				auto& texCoordAccessor = asset->accessors[texcoord->accessorIndex];
				/*if (!texCoordAccessor.bufferViewIndex.has_value())
					continue;*/

				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(*asset, texCoordAccessor, [&](fastgltf::math::fvec2 uv, std::size_t idx) {
					vertexData[idx].UV = Asset::PackUV({ uv.x(), uv.y() });
					});
			}

			auto& indexAccessor = asset->accessors[primitive.indicesAccessor.value()];
			/*if (!indexAccessor.bufferViewIndex.has_value())
				return false;*/


			std::vector<uint32_t> indices(indexAccessor.count);
			if (indexAccessor.componentType == fastgltf::ComponentType::UnsignedByte || indexAccessor.componentType == fastgltf::ComponentType::UnsignedShort)
			{
				// Only 32bit indices are supported. With mesh shaders its not gonna matter anyways since the indices will be remapped to 8bits.
				std::vector<uint16_t> tempIndices(indexAccessor.count);
				fastgltf::copyFromAccessor<uint16_t>(*asset, indexAccessor, tempIndices.data());
				for (int j = 0; j < tempIndices.size(); j++) {
					indices[j] = tempIndices[j] + baseIndex;
				}
			}
			else
			{
				fastgltf::copyFromAccessor<uint32_t>(*asset, indexAccessor, indices.data());
			}

			Asset::LegacySubMesh newSubmesh;
			newSubmesh.Bounds = Asset::ComputeBoundingSphere(vertexData);
			newSubmesh.IndexOffset = meshData.Indices.size();
			newSubmesh.IndexCount = indices.size();
			baseIndex += indices.size();
			meshData.Submeshes[i] = newSubmesh;

			meshData.Vertices.insert(meshData.Vertices.end(), vertexData.begin(), vertexData.end());
			meshData.Indices.insert(meshData.Indices.end(), indices.begin(), indices.end());
		}

		return meshData;
	}

	inline aZero::Asset::MeshletMeshData ExtractGltfMeshletMesh(const fastgltf::Mesh& mesh, fastgltf::Asset* asset)
	{
		const uint32_t numPrimitives = std::clamp((uint32_t)mesh.primitives.size(), 0u, aZero::Asset::g_MaxNumSubmeshes); // Currently only supports atmost 10 submeshes
		aZero::Asset::MeshletMeshData meshData;
		meshData.Name = mesh.name.c_str();
		meshData.Submeshes.resize(numPrimitives);

		uint32_t vertexOffset = 0;
		for (int i = 0; i < numPrimitives; i++)
		{
			const auto& primitive = mesh.primitives[i];
			size_t baseColorTexcoordIndex = 0;

			if (primitive.materialIndex.has_value())
			{
				const fastgltf::Material& material = asset->materials[primitive.materialIndex.value()];
				if (material.pbrData.baseColorTexture.has_value())
				{
					if (material.pbrData.baseColorTexture->transform && material.pbrData.baseColorTexture->transform->texCoordIndex.has_value()) {
						baseColorTexcoordIndex = material.pbrData.baseColorTexture->transform->texCoordIndex.value();
					}
					else {
						baseColorTexcoordIndex = material.pbrData.baseColorTexture->texCoordIndex;
					}
				}
			}

			auto& positionAccessor = asset->accessors[primitive.findAttribute("POSITION")->accessorIndex];
			/*if (!positionAccessor.bufferViewIndex.has_value())
				continue;*/

			std::vector<Asset::Vertex> vertexData(positionAccessor.count);
			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(*asset, positionAccessor, [&vertexData](fastgltf::math::fvec3 pos, std::size_t idx) {
				auto position = fastgltf::math::fvec3(pos.x(), pos.y(), pos.z());
				vertexData[idx].Position = { position.x(), position.y(), position.z() };
				});

			auto& normalAccessor = asset->accessors[primitive.findAttribute("NORMAL")->accessorIndex];
			/*if (!normalAccessor.bufferViewIndex.has_value())
				continue;*/

			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(*asset, normalAccessor, [&vertexData](fastgltf::math::fvec3 n, std::size_t idx) {
				vertexData[idx].Normal = Asset::PackNormal(Asset::EncodeNormalOctahedral({ n.x(), n.y(), n.z() }));
				});

			const std::string texcoordAttribute = std::string("TEXCOORD_") + std::to_string(baseColorTexcoordIndex);
			if (const auto* texcoord = primitive.findAttribute(texcoordAttribute); texcoord != primitive.attributes.end()) {
				// Tex coord
				auto& texCoordAccessor = asset->accessors[texcoord->accessorIndex];
				/*if (!texCoordAccessor.bufferViewIndex.has_value())
					continue;*/

				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(*asset, texCoordAccessor, [&](fastgltf::math::fvec2 uv, std::size_t idx) {
					vertexData[idx].UV = Asset::PackUV({ uv.x(), uv.y() });
					});
			}

			auto& indexAccessor = asset->accessors[primitive.indicesAccessor.value()];
			/*if (!indexAccessor.bufferViewIndex.has_value())
				return false;*/


			std::vector<uint32_t> indices(indexAccessor.count);
			if (indexAccessor.componentType == fastgltf::ComponentType::UnsignedByte || indexAccessor.componentType == fastgltf::ComponentType::UnsignedShort)
			{
				// Only 32bit indices are supported. With mesh shaders its not gonna matter anyways since the indices will be remapped to 8bits.
				std::vector<uint16_t> tempIndices(indexAccessor.count);
				fastgltf::copyFromAccessor<uint16_t>(*asset, indexAccessor, tempIndices.data());
				for (int j = 0; j < tempIndices.size(); j++) {
					indices[j] = tempIndices[j];
				}
			}
			else
			{
				fastgltf::copyFromAccessor<uint32_t>(*asset, indexAccessor, indices.data());
			}

			std::vector<Asset::Meshlet> meshlets;
			std::vector<BoundingVolume::BoundingSphere> meshletBounds;
			Asset::Meshletize(vertexData, indices, meshlets, meshletBounds, vertexOffset);

			Asset::MeshletSubMesh newSubmesh;
			newSubmesh.Bounds = Asset::ComputeBoundingSphere(vertexData);
			newSubmesh.MeshletOffset = meshData.Meshlets.size();
			newSubmesh.MeshletCount = meshlets.size();
			vertexOffset += vertexData.size();
			meshData.Submeshes[i] = newSubmesh;

			meshData.Vertices.insert(meshData.Vertices.end(), vertexData.begin(), vertexData.end());
			meshData.Meshlets.insert(meshData.Meshlets.end(), meshlets.begin(), meshlets.end());
			meshData.MeshletBounds.insert(meshData.MeshletBounds.end(), meshletBounds.begin(), meshletBounds.end());
		}

		return meshData;
	}

	inline aZero::Asset::ImageData ExtractGltfImage(const fastgltf::Image& image, fastgltf::Asset* asset, const std::filesystem::path& path)
	{
		aZero::Asset::ImageData imageData;

		std::visit(fastgltf::visitor{
			[](const auto& arg) {},
			[&imageData, &path](const fastgltf::sources::URI& filePath) {
				// todo Figure out how to handle formatting
				auto res = aZero::File::LoadImageData(path.parent_path() / std::filesystem::path(filePath.uri.path()));
				if (res.has_value())
				{
					imageData = std::move(res.value());
				}
			},
			[&imageData, &image](const fastgltf::sources::Array& vector) {
				imageData = aZero::File::LoadImageDataFromMemory(image.name.c_str(), vector.bytes.data(), vector.bytes.size(), 0);
			},
			[asset, &imageData, &image](const fastgltf::sources::BufferView& view) {
				auto& bufferView = asset->bufferViews[view.bufferViewIndex];
				auto& buffer = asset->buffers[bufferView.bufferIndex];
				std::visit(fastgltf::visitor{
					[](const auto& arg) {},
					[&](const fastgltf::sources::Array& vector) {
						imageData = aZero::File::LoadImageDataFromMemory(image.name.c_str(), vector.bytes.data(), bufferView.byteLength, bufferView.byteOffset);
					}
				}, buffer.data);
			}
			}, image.data);

		return imageData;
	}
}