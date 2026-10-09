#pragma once
#include <numbers>
#include <array>
#include <filesystem>

#include "IO_Helper.hpp"
#include "UUID.hpp"
#include "AssetData.hpp"

namespace aZero::Package
{
	class AssetPackage
	{
		static constexpr uint16_t s_VERSION = 0;
		static constexpr uint16_t s_VERIFICATION = 1337;

		template<typename T>
		struct AssetEntry
		{
			std::vector<std::array<char, aZero::Helper::UUID::GetSizeAsString()>> UUIDs;
			std::vector<uint32_t> Index_to_AssetOffset;
			std::vector<uint64_t> Offsets;
		};

	public:
		struct WriteData
		{
			std::vector<aZero::Asset::MeshletMeshData> MeshletMeshes;
			std::vector<aZero::Asset::ImageData> Images;
		};

		class Header
		{
			friend class AssetPackage;
		public:
			Header() = default;
			[[nodiscard]] uint16_t GetVersion() const { return m_Version; }
			[[nodiscard]] uint64_t GetTotalAssetSize() const { return m_TotalAssetSize; }
		private:
			uint16_t m_Verification;
			uint16_t m_Version;
			uint64_t m_TotalAssetSize;
		};

		class Body
		{
			friend class AssetPackage;
		private:
			std::tuple<AssetEntry<aZero::Asset::MeshletMeshData>, AssetEntry<aZero::Asset::ImageData>> AssetEntries;
			std::vector<std::byte> Data;
		};

		AssetPackage() = default;
		AssetPackage(const std::filesystem::path& path)
		{
			this->Unpack(path);
		}

		bool Pack(const std::filesystem::path& path, const WriteData& data, bool overwrite = true)
		{
			const std::ios::openmode openMode = overwrite ? std::ios::binary | std::ios::trunc : std::ios::binary;
			std::ofstream file(path, openMode);
			if (!file.is_open())
			{
				return false;
			}

			Header header;
			header.m_Verification = s_VERIFICATION;
			header.m_Version = s_VERSION;
			file.write(reinterpret_cast<char*>(&header), sizeof(header));

			Body body;

			// Write asset data
			auto assetStart = file.tellp();
			this->WriteAssets(body, data.MeshletMeshes, file);
			this->WriteAssets(body, data.Images, file);
			header.m_TotalAssetSize = static_cast<uint64_t>(file.tellp() - assetStart);

			// Write asset entries vectors
			this->WriteAssetEntry<aZero::Asset::MeshletMeshData>(body, file);
			this->WriteAssetEntry<aZero::Asset::ImageData>(body, file);

			file.seekp(0);
			file.write(reinterpret_cast<char*>(&header), sizeof(header)); // We need to write again since we don't have TotalAssetSize until later

			file.close();

			return true;
		}

		bool Unpack(const std::filesystem::path& path)
		{
			std::ifstream file(path, std::ios::binary);
			if (!file.is_open())
			{
				return false;
			}

			std::streampos start = file.tellg();
			file.seekg(0, std::ios::end);
			if (file.tellg() - start < sizeof(Header))
			{
				return false;
			}
			file.seekg(start);

			Header header;
			file.read(reinterpret_cast<char*>(&header), sizeof(Header));

			if (header.m_Verification != s_VERIFICATION)
			{
				return false;
			}

			Body body;
			body.Data.resize(header.m_TotalAssetSize);
			file.read(reinterpret_cast<char*>(body.Data.data()), header.m_TotalAssetSize);
			this->ReadAssetEntry<aZero::Asset::MeshletMeshData>(body, file);
			this->ReadAssetEntry<aZero::Asset::ImageData>(body, file);

			file.close();

			m_Header = std::move(header);
			m_Body = std::move(body);

			return true;
		}

		void PrintContent()
		{
			std::cout << "-------------- HEADER --------------\n";
			std::cout << "Version: " << m_Header.GetVersion() << "\n";
			std::cout << "Total asset data size: " << m_Header.GetTotalAssetSize() << "\n";

			std::cout << "\n--------------  BODY  --------------\n";
			std::cout << "Meshlet Mesh Count: " << this->GetAssetCount<aZero::Asset::MeshletMeshData>() << "\n";
			std::cout << "Image Count: " << this->GetAssetCount<aZero::Asset::ImageData>() << "\n";

			std::cout << "Meshlet Mesh with UUID Count: " << std::get<AssetEntry<aZero::Asset::MeshletMeshData>>(m_Body.AssetEntries).Index_to_AssetOffset.size() << "\n";
			std::cout << "Image with UUID Count : " << std::get<AssetEntry<aZero::Asset::ImageData>>(m_Body.AssetEntries).Index_to_AssetOffset.size() << "\n";
			std::cout << "------------------------------------\n";
		}

		template<typename AssetType>
		[[nodiscard]] AssetType GetAsset(uint32_t index) const
		{
			auto& assetEntry = std::get<AssetEntry<AssetType>>(m_Body.AssetEntries);
			if (index >= assetEntry.Offsets.size())
			{
				throw std::invalid_argument("Index out-of-range.");
			}

			const auto& offset = assetEntry.Offsets[index];
			AssetType asset;
			this->ReadAsset(asset, m_Body.Data, offset);
			return asset;
		}

		template<typename AssetType>
		[[nodiscard]] uint32_t GetAssetCount() const { return std::get<AssetEntry<AssetType>>(m_Body.AssetEntries).Offsets.size(); }

		template<typename AssetType>
		[[nodiscard]] std::tuple<std::vector<uint32_t>, std::vector<std::array<char, aZero::Helper::UUID::GetSizeAsString()>>> GetAssetUUID_Map() const {
			return { std::get<AssetEntry<AssetType>>(m_Body.AssetEntries).Index_to_AssetOffset, std::get<AssetEntry<AssetType>>(m_Body.AssetEntries).UUIDs };
		}

		[[nodiscard]] uint16_t GetVersion() const { return m_Header.GetVersion(); }

	private:

		void ReadBase(const std::vector<std::byte>& data, uint64_t& offset, std::string& name, std::array<char, aZero::Helper::UUID::GetSizeAsString()>& uuid) const
		{
			name = aZero::File::ReadString(data, offset);
			std::memcpy(uuid.data(), &data[offset], aZero::Helper::UUID::GetSizeAsString());
			offset += aZero::Helper::UUID::GetSizeAsString();
		}

		void WriteBase(const std::string& name, const std::array<char, aZero::Helper::UUID::GetSizeAsString()>& uuid, std::ofstream& file) const
		{
			aZero::File::WriteString(name, file);
			file.write(uuid.data(), aZero::Helper::UUID::GetSizeAsString());
		}

		// IMAGE
		void ReadAsset(aZero::Asset::ImageData& image, const std::vector<std::byte>& data, uint64_t baseOffset) const
		{
			uint64_t offset = baseOffset;
			ReadBase(data, offset, image.Name, image.UUID);
			image.Data = aZero::File::ReadVector<std::byte>(data, offset);
			image.MipLevels = aZero::File::ReadVector<aZero::Asset::ImageData::Mip>(data, offset);

			std::memcpy(&image.BaseWidth, &data[offset], sizeof(image.BaseWidth));
			offset += sizeof(image.BaseWidth);

			std::memcpy(&image.BaseHeight, &data[offset], sizeof(image.BaseHeight));
			offset += sizeof(image.BaseHeight);

			std::memcpy(&image.FaceCount, &data[offset], sizeof(image.FaceCount));
			offset += sizeof(image.FaceCount);

			std::memcpy(&image.DepthOrArrayCount, &data[offset], sizeof(image.DepthOrArrayCount));
			offset += sizeof(image.DepthOrArrayCount);

			uint32_t format;
			std::memcpy(&format, &data[offset], sizeof(uint32_t));
			image.Format = static_cast<VkFormat>(format);
			offset += sizeof(uint32_t);
			std::memcpy(&image.Type, &data[offset], sizeof(uint32_t));
		}

		void WriteAsset(const aZero::Asset::ImageData& image, std::ofstream& file) const
		{
			WriteBase(image.Name, image.UUID, file);
			aZero::File::WriteVector(image.Data, file);
			aZero::File::WriteVector(image.MipLevels, file);

			file.write(reinterpret_cast<const char*>(&image.BaseWidth), sizeof(image.BaseWidth));
			file.write(reinterpret_cast<const char*>(&image.BaseHeight), sizeof(image.BaseHeight));

			file.write(reinterpret_cast<const char*>(&image.FaceCount), sizeof(image.FaceCount));
			file.write(reinterpret_cast<const char*>(&image.DepthOrArrayCount), sizeof(image.DepthOrArrayCount));

			const uint32_t format32u = static_cast<uint32_t>(image.Format);
			file.write(reinterpret_cast<const char*>(&format32u), sizeof(uint32_t));

			const uint32_t type = static_cast<uint32_t>(image.Type);
			file.write(reinterpret_cast<const char*>(&type), sizeof(uint32_t));
		}

		// MESHLET MESH
		void ReadAsset(aZero::Asset::MeshletMeshData& mesh, const std::vector<std::byte>& data, uint64_t baseOffset) const
		{
			uint64_t offset = baseOffset;
			ReadBase(data, offset, mesh.Name, mesh.UUID);
			mesh.Meshlets = aZero::File::ReadVector<aZero::Asset::Meshlet>(data, offset);
			mesh.MeshletBounds = aZero::File::ReadVector<BoundingVolume::BoundingSphere>(data, offset);
			mesh.Vertices = aZero::File::ReadVector<aZero::Asset::Vertex>(data, offset);
			mesh.Submeshes = aZero::File::ReadVector<aZero::Asset::MeshletSubMesh>(data, offset);
		}

		void WriteAsset(const aZero::Asset::MeshletMeshData& mesh, std::ofstream& file) const
		{
			WriteBase(mesh.Name, mesh.UUID, file);
			aZero::File::WriteVector(mesh.Meshlets, file);
			aZero::File::WriteVector(mesh.MeshletBounds, file);
			aZero::File::WriteVector(mesh.Vertices, file);
			aZero::File::WriteVector(mesh.Submeshes, file);
		}

		//

		template<typename T>
		void WriteAssets(Body& body, const std::vector<T>& assetVector, std::ofstream& file) const
		{
			AssetEntry<T>& assetEntry = std::get<AssetEntry<T>>(body.AssetEntries);
			assetEntry.Offsets.reserve(assetVector.size());
			for (uint32_t i = 0; i < assetVector.size(); i++)
			{
				const T& asset = assetVector[i];
				const uint64_t offset = static_cast<uint64_t>(file.tellp()) - static_cast<uint64_t>(sizeof(Header));
				assetEntry.Offsets.push_back(offset);

				if (!asset.UUID.empty())
				{
					assetEntry.Index_to_AssetOffset.push_back(i);
					assetEntry.UUIDs.push_back({});
					std::memcpy(&assetEntry.UUIDs[assetEntry.UUIDs.size() - 1], asset.UUID.data(), aZero::Helper::UUID::GetSizeAsString());
				}
				this->WriteAsset(asset, file);
			}
		}

		template<typename T>
		void WriteAssetEntry(const Body& body, std::ofstream& file) const
		{
			aZero::File::WriteVector(std::get<AssetEntry<T>>(body.AssetEntries).Offsets, file);
			aZero::File::WriteVector(std::get<AssetEntry<T>>(body.AssetEntries).Index_to_AssetOffset, file);
			aZero::File::WriteVector(std::get<AssetEntry<T>>(body.AssetEntries).UUIDs, file);
		}

		template<typename T>
		void ReadAssetEntry(Body& body, std::ifstream& file) const
		{
			std::get<AssetEntry<T>>(body.AssetEntries).Offsets
				= aZero::File::ReadVector<typename decltype(std::get<AssetEntry<T>>(body.AssetEntries).Offsets)::value_type>(file);

			std::get<AssetEntry<T>>(body.AssetEntries).Index_to_AssetOffset
				= aZero::File::ReadVector<typename decltype(std::get<AssetEntry<T>>(body.AssetEntries).Index_to_AssetOffset)::value_type>(file);

			std::get<AssetEntry<T>>(body.AssetEntries).UUIDs
				= aZero::File::ReadVector<typename decltype(std::get<AssetEntry<T>>(body.AssetEntries).UUIDs)::value_type>(file);
		}

		Header m_Header;
		Body m_Body;
	};
}
