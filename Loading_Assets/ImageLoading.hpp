#pragma once
#include <filesystem>
#include <string>
#include "AssetData.hpp"
#include "stb_image.h"

namespace aZero::File
{
	inline std::optional<aZero::Asset::ImageData> LoadImageData(const std::filesystem::path& path, bool generateMips = true)
	{
		if (path.extension() == ".ktx2")
		{
			aZero::Asset::ImageData imageData;

			ktxTexture2* ktxTex = nullptr;
			KTX_error_code result =
				ktxTexture2_CreateFromNamedFile(
					path.generic_string().c_str(),
					KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
					&ktxTex);

			if (result != KTX_SUCCESS)
			{
				return {};
			}

			if (ktxTexture2_NeedsTranscoding(ktxTex)) {

				const bool isSRGB = ktxTexture2_GetTransferFunction_e(ktxTex) == khr_df_transfer_e::KHR_DF_TRANSFER_SRGB;

				result = ktxTexture2_TranscodeBasis( // todo Maybe we need to know if its srgb or not...?
					ktxTex,
					KTX_TTF_BC7_RGBA,
					KTX_TF_HIGH_QUALITY);

				if (result != KTX_SUCCESS)
				{
					ktxTexture2_Destroy(ktxTex);
					return {};
				}

				imageData.Format = isSRGB ? aZero::RenderAPI::EImageFormat::BC7_UNORM_SRGB : aZero::RenderAPI::EImageFormat::BC7_UNORM;
			}
			else {
				imageData.Format = aZero::RenderAPI::FromVK_Format(ktxTex->vkFormat);
			}

			if (ktxTex->isCubemap) {
				imageData.Type = ktxTex->isArray ? aZero::RenderAPI::EImageType::ImageCube_Array : aZero::RenderAPI::EImageType::ImageCube;
			}
			else
			{
				if (ktxTex->baseDepth > 1) {
					imageData.Type = aZero::RenderAPI::EImageType::Image3D;
				}
				else if (ktxTex->baseHeight > 1) {
					imageData.Type = ktxTex->isArray ? aZero::RenderAPI::EImageType::Image2D_Array : aZero::RenderAPI::EImageType::Image2D;
				}
				else {
					imageData.Type = ktxTex->isArray ? aZero::RenderAPI::EImageType::Image1D_Array : aZero::RenderAPI::EImageType::Image1D;
				}
			}

			imageData.FaceCount = ktxTex->numFaces;
			imageData.BaseWidth = ktxTex->baseWidth;
			imageData.BaseHeight = ktxTex->baseHeight;
			imageData.DepthOrArrayCount = ktxTex->isArray // Is array?
				? ktxTex->numLayers // Yes => Array count
				: imageData.Type == aZero::RenderAPI::EImageType::Image3D ? // No => Is it 3D? Yes => Get depth, No => Set 0 as depth.
				ktxTex->baseDepth : 1.f;

			imageData.MipLevels.resize(ktxTex->numLevels);
			for (uint32_t mip = 0; mip < ktxTex->numLevels; mip++)
			{
				ktx_size_t offset;

				ktxTexture_GetImageOffset(
					reinterpret_cast<ktxTexture*>(ktxTex),
					mip,
					0,
					0,
					&offset);

				const ktx_size_t size = ktxTexture_GetImageSize(
					reinterpret_cast<ktxTexture*>(ktxTex),
					mip);

				// Reduces textures mip0 width and height by a power of 2 per mip level
				uint32_t mipWidth = std::max(1u, ktxTex->baseWidth >> mip);
				uint32_t mipHeight = std::max(1u, ktxTex->baseHeight >> mip);

				imageData.MipLevels[mip].Offset = offset;
				imageData.MipLevels[mip].SlicePitch = size;

				if (imageData.Format == aZero::RenderAPI::EImageFormat::BC7_UNORM ||
					imageData.Format == aZero::RenderAPI::EImageFormat::BC7_UNORM_SRGB)
				{
					// BC7 - Stores the texture in blocks of 16 bytes.
					// We calculate the number of blocks and multiply by 16 to get the width.
					// We add 3 so we always will round up. Ex.
					/*
						width = 10
						10 / 4 = 2.5
						integers are truncated so we will get 8 (4*2) which isnt the correct rowpitch (it should be 10)
						we add 3 to always get a value that when rounded down becomes atleast the same as width
						Ex:
						width = 10
						(10 + 3) / 4
						13 / 4 = 3.25 => 3
						4 * 3 = 12 and now we have the same or more than 10.

					*/
					imageData.MipLevels[mip].RowPitch =
						((mipWidth + 3) / 4) * 16;
				}
				else
				{

					imageData.MipLevels[mip].RowPitch = mipWidth * 4;
				}
			}
			imageData.Data.resize(ktxTex->dataSize);
			std::memcpy(imageData.Data.data(), ktxTex->pData, ktxTex->dataSize);

			imageData.Name = path.filename().generic_string();
			
			ktxTexture2_Destroy(ktxTex);

			return imageData;
		}
		else if (path.extension() == ".png" || path.extension() == ".jpg" || path.extension() == ".jpeg")
		{
			aZero::Asset::ImageData imageData;

			std::int32_t width, height, channels;
			stbi_uc* loadedImage = stbi_load(path.generic_string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
			if (!loadedImage)
			{
				return {};
			}

			imageData.Type = aZero::RenderAPI::EImageType::Image2D;
			imageData.FaceCount = 1;
			imageData.BaseWidth = width;
			imageData.BaseHeight = height;
			imageData.DepthOrArrayCount = 1;
			imageData.Name = path.filename().generic_string();

			imageData.Data.resize(width * height * STBI_rgb_alpha);
			memcpy(imageData.Data.data(), loadedImage, imageData.Data.size());

			aZero::Asset::ImageData::Mip miplevel;
			miplevel.Offset = 0;
			miplevel.RowPitch = width * sizeof(DWORD);
			miplevel.SlicePitch = miplevel.RowPitch * height;
			imageData.MipLevels.emplace_back(miplevel);

			if (path.filename().generic_string().ends_with(std::string("_BaseColor") + path.extension().generic_string()))
			{
				imageData.Format = aZero::RenderAPI::EImageFormat::RGBA8_UNORM_SRGB;
			}
			else
			{
				imageData.Format = aZero::RenderAPI::EImageFormat::RGBA8_UNORM;
			}

			stbi_image_free(loadedImage);

			return imageData;
		}

		return {};
	}

	inline aZero::Asset::ImageData LoadImageDataFromMemory(const std::filesystem::path& path, const std::byte* memoryPtr, std::size_t numBytes, std::size_t offsetIntoMemoryPtr = 0)
	{
		aZero::Asset::ImageData imageData;

		std::int32_t width, height, channels;
		stbi_uc* loadedImage = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(memoryPtr + offsetIntoMemoryPtr),
			static_cast<int32_t>(numBytes), &width, &height, &channels, STBI_rgb_alpha);

		imageData.Type = aZero::RenderAPI::EImageType::Image2D;
		imageData.FaceCount = 1;
		imageData.BaseWidth = width;
		imageData.BaseHeight = height;
		imageData.DepthOrArrayCount = 1;
		imageData.Name = path.filename().generic_string();

		imageData.Data.resize(width * height * STBI_rgb_alpha);
		memcpy(imageData.Data.data(), loadedImage, imageData.Data.size());

		aZero::Asset::ImageData::Mip miplevel;
		miplevel.Offset = 0;
		miplevel.RowPitch = width * sizeof(DWORD);
		miplevel.SlicePitch = miplevel.RowPitch * height;
		imageData.MipLevels.emplace_back(miplevel);

		if (path.filename().generic_string().ends_with(std::string("_BaseColor") + path.extension().generic_string()))
		{
			imageData.Format = aZero::RenderAPI::EImageFormat::RGBA8_UNORM_SRGB;
		}
		else
		{
			imageData.Format = aZero::RenderAPI::EImageFormat::RGBA8_UNORM;
		}

		return imageData;
	}
}