#pragma once
#include <meshoptimizer.h>
#include <array>
#include "BoundingVolume.hpp"

namespace aZero::Asset
{
	static inline constexpr std::uint32_t g_VerticesPerMeshlet = 64;
	static inline constexpr std::uint32_t g_PrimitivesPerMeshlet = 84;
	static inline constexpr std::uint32_t g_MaxNumSubmeshes = 10;

	struct Vertex
	{
		glm::vec3 Position;
		uint32_t UV;
		uint32_t Normal;
	};

	using VertexIndex = std::uint32_t; // For use with index buffers

	struct Meshlet
	{
		std::uint32_t VertexOffset;
		std::uint32_t VertexCount;
		std::uint32_t PrimitiveCount;
		std::array<std::uint32_t, g_PrimitivesPerMeshlet> Primitives;
	};

	struct MeshletSubMesh
	{
		std::uint32_t MeshletOffset;
		std::uint32_t MeshletCount;
		BoundingVolume::BoundingSphere Bounds;
	};

	struct LegacySubMesh
	{
		std::uint32_t IndexOffset;
		std::uint32_t IndexCount;
		BoundingVolume::BoundingSphere Bounds;
	};

	inline uint32_t Pack8To32(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
	{
		return
			(static_cast<uint32_t>(d) << 24) |
			(static_cast<uint32_t>(c) << 16) |
			(static_cast<uint32_t>(b) << 8) |
			(static_cast<uint32_t>(a));
	}

	inline uint16_t FloatToUNorm16(float v)
	{
		v = std::clamp(v, 0.0f, 1.0f);
		return (uint16_t)(v * 65535.0f + 0.5f);
	}

	inline uint32_t PackNormal(const glm::vec2& normal)
	{
		uint16_t n0 = FloatToUNorm16(normal.x * 0.5f + 0.5f);
		uint16_t n1 = FloatToUNorm16(normal.y * 0.5f + 0.5f);
		return uint32_t(n0) | (uint32_t(n1) << 16);
	}

	inline uint32_t PackUV(const glm::vec2& uv)
	{
		uint16_t u = Helpers::float_to_half(uv.x);
		uint16_t v = Helpers::float_to_half(uv.y);

		return uint32_t(u) | (uint32_t(v) << 16);
	}

	inline glm::vec2 EncodeNormalOctahedral(const glm::vec3& n)
	{
		float invL1 = 1.0f / (std::fabs(n.x) + std::fabs(n.y) + std::fabs(n.z));
		float x = n.x * invL1;
		float y = n.y * invL1;
		float z = n.z * invL1;

		glm::vec2 enc = { x, y };

		if (z < 0.0f)
		{
			float oldX = enc.x;
			float oldY = enc.y;

			enc.x = (1.0f - std::fabs(oldY)) * (oldX >= 0.0f ? 1.0f : -1.0f);
			enc.y = (1.0f - std::fabs(oldX)) * (oldY >= 0.0f ? 1.0f : -1.0f);
		}

		return enc;
	}

	inline Vertex PackVertex(const glm::vec2& normal, const glm::vec2& uv)
	{
		Vertex p{ };

		p.Normal = PackNormal(normal);
		p.UV = PackUV(uv);

		return p;
	}

	inline BoundingVolume::BoundingSphere ComputeBoundingSphere(const std::vector<Vertex>& points)
	{
		glm::vec3 p0 = { points[0].Position.x, points[0].Position.y, points[0].Position.z };

		int i1 = 0;
		float maxDist = 0.0f;

		for (int i = 0; i < points.size(); i++)
		{
			float d = glm::length2(glm::vec3(points[i].Position.x, points[i].Position.y, points[i].Position.z) - p0);
			
			if (d > maxDist)
			{
				maxDist = d;
				i1 = i;
			}
		}

		int i2 = i1;
		maxDist = 0.0f;

		for (int i = 0; i < points.size(); i++)
		{
			float d = glm::length2(glm::vec3(points[i].Position.x, points[i].Position.y, points[i].Position.z) - glm::vec3(points[i1].Position.x, points[i1].Position.y, points[i1].Position.z));
			if (d > maxDist)
			{
				maxDist = d;
				i2 = i;
			}
		}

		glm::vec3 center = glm::vec3(points[i1].Position.x, points[i1].Position.y, points[i1].Position.z) + glm::vec3(points[i2].Position.x, points[i2].Position.y, points[i2].Position.z) * 0.5f;
		float radius = glm::length(glm::vec3(points[i2].Position.x, points[i2].Position.y, points[i2].Position.z) - center);

		for (const auto& p : points)
		{
			glm::vec3 d = p.Position - center;
			float dist = glm::length(d);

			if (dist > radius)
			{
				float newRadius = (radius + dist) * 0.5f;
				float k = (newRadius - radius) / dist;

				center += d * k;
				radius = newRadius;
			}
		}

		return { glm::vec3(center.x, center.y, center.z), radius };
	}

	inline void Meshletize( // Named it myself :))
		std::vector<aZero::Asset::Vertex>& vertices,
		std::vector<aZero::Asset::VertexIndex>& indices,
		std::vector<aZero::Asset::Meshlet>& outMeshlets,
		std::vector<BoundingVolume::BoundingSphere>& outMeshletBounds,
		uint32_t vertexBaseOffset
	)
	{
		const size_t max_vertices = aZero::Asset::g_VerticesPerMeshlet;
		const size_t max_triangles = aZero::Asset::g_PrimitivesPerMeshlet;

		const size_t max_meshlets = meshopt_buildMeshletsBound(
			indices.size(), max_vertices, max_triangles);

		meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertices.size());
		meshopt_optimizeOverdraw(indices.data(), indices.data(), indices.size(), &vertices[0].Position.x, vertices.size(), sizeof(vertices[0]), 1.05f);

		meshopt_optimizeVertexFetch(
			vertices.data(),
			indices.data(),
			indices.size(),
			vertices.data(),
			vertices.size(),
			sizeof(vertices[0])
		);

		meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertices.size());

		std::vector<meshopt_Meshlet> tempMeshlets;
		std::vector<aZero::Asset::VertexIndex> local_indices;
		std::vector<uint8_t> primitives;
		tempMeshlets.resize(max_meshlets);
		local_indices.resize(max_meshlets * max_vertices);
		primitives.resize(max_meshlets * max_triangles * 3);
		size_t meshlet_count = meshopt_buildMeshlets(tempMeshlets.data(), local_indices.data(), primitives.data(), indices.data(),
			indices.size(), &vertices[0].Position.x, vertices.size(), sizeof(vertices[0]), max_vertices, max_triangles, 0.f);

		const meshopt_Meshlet& last = tempMeshlets[meshlet_count - 1];

		local_indices.resize(last.vertex_offset + last.vertex_count);
		primitives.resize(last.triangle_offset + last.triangle_count * 3);
		tempMeshlets.resize(meshlet_count);

		outMeshlets.reserve(meshlet_count);

		std::vector<aZero::Asset::Vertex> outVertices;
		for (uint32_t i = 0; i < meshlet_count; i++)
		{
			const meshopt_Meshlet& meshlet = tempMeshlets[i];
			aZero::Asset::Meshlet newMeshlet;

			meshopt_Bounds bounds = meshopt_computeMeshletBounds(&local_indices[meshlet.vertex_offset], &primitives[meshlet.triangle_offset],
				meshlet.triangle_count, &vertices[0].Position.x, vertices.size(), sizeof(vertices[0]));
			outMeshletBounds.emplace_back(glm::vec3(bounds.center[0], bounds.center[1], bounds.center[2]), bounds.radius);

			newMeshlet.PrimitiveCount = meshlet.triangle_count;
			newMeshlet.VertexCount = meshlet.vertex_count;

			for (uint32_t h = 0; h < meshlet.vertex_count; h++)
			{
				outVertices.push_back(vertices[local_indices[meshlet.vertex_offset + h]]);
			}

			for (uint32_t j = 0; j < meshlet.triangle_count; j++)
			{
				const uint32_t primOffset = meshlet.triangle_offset + j * 3;
				newMeshlet.Primitives[j] = Pack8To32(primitives[primOffset], primitives[primOffset + 1], primitives[primOffset + 2], 0);
			}

			newMeshlet.VertexOffset = meshlet.vertex_offset + vertexBaseOffset;
			outMeshlets.emplace_back(newMeshlet);
		}

		vertices = outVertices;
	}
}