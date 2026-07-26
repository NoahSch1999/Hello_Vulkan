#include <iostream>
#include "AssetPackage.hpp"
#include "ImageLoading.hpp"
#include "Gltf.hpp"

int main()
{
    auto imageloaded = aZero::File::LoadImageData("C:/Projects/Programming/Sandbox_CMake/Loading_Assets/arch_stone_wall_01_BaseColor.png");
    if (!imageloaded.has_value())
    {
        std::cout << "sads";
    }
    {
        aZero::Package::AssetPackage::WriteData data;
        {
            aZero::Asset::MeshletMeshData mesh;
            aZero::Asset::Meshlet meshlet{ .VertexOffset = 4, .VertexCount = 3, .PrimitiveCount = 2, .Primitives = {11,22,33,44} };
            aZero::Asset::Vertex vertex{ .Position = {111,222,333 }, .UV = {444, 555}, .Normal = {666, 777} };
            aZero::Asset::MeshletSubMesh submesh{ .MeshletOffset = 1, .MeshletCount = 1, .Bounds = {{0, 1, 2}, 1337 } };
            mesh.Meshlets.push_back(meshlet);
            mesh.Vertices.push_back(vertex);
            mesh.MeshletBounds.push_back({ {0, 1, 2}, 1337 });
            mesh.Submeshes.push_back(submesh);
            mesh.Name = "MY MESH";
            mesh.UUID = aZero::Helper::CreateUUID().value().ToCharArray();
            data.MeshletMeshes.push_back(mesh);
        }

        {
            aZero::Asset::MeshletMeshData mesh;
            aZero::Asset::Meshlet meshlet{ .VertexOffset = 4, .VertexCount = 3, .PrimitiveCount = 2, .Primitives = {11,22,33,44} };
            aZero::Asset::Vertex vertex{ .Position = {111,222,333 }, .UV = {444, 555}, .Normal = {666, 777} };
            aZero::Asset::MeshletSubMesh submesh{ .MeshletOffset = 1, .MeshletCount = 1, .Bounds = {{0, 1, 2}, 1337 } };
            mesh.Meshlets.push_back(meshlet);
            mesh.Vertices.push_back(vertex);
            mesh.MeshletBounds.push_back({ {0, 1, 2}, 1337 });
            mesh.Submeshes.push_back(submesh);
            mesh.Name = "MY SECOND MESH";
            mesh.UUID = aZero::Helper::CreateUUID().value().ToCharArray();
            data.MeshletMeshes.push_back(mesh);
        }

        aZero::Asset::ImageData image;
        image.Format = aZero::RenderAPI::EImageFormat::RGBA8_UNORM_SRGB;
        image.Type = aZero::Asset::EImageType::Image1D;
        image.Data.push_back({});
        image.Name = "MY IMAGE";
        aZero::Asset::ImageData::Mip mip{ .Offset = 2, .RowPitch = 3, .SlicePitch = 3 };
        image.MipLevels.push_back(mip);
        image.UUID = aZero::Helper::CreateUUID().value().ToCharArray();

        aZero::Package::AssetPackage pack;
        data.Images.push_back(image);

        pack.Pack("package.pak", data, true);
    }

    {
        aZero::Package::AssetPackage pack;
        if (pack.Unpack("package.pak"))
        {
            pack.PrintContent();
            aZero::Asset::MeshletMeshData mesh = pack.GetAsset<aZero::Asset::MeshletMeshData>(0);
            int x = mesh.Meshlets.size();

            aZero::Asset::MeshletMeshData mesh2 = pack.GetAsset<aZero::Asset::MeshletMeshData>(1);
            x = mesh2.Meshlets.size();

            auto off = pack.GetAssetCount<aZero::Asset::MeshletMeshData>();
            auto [i, j] = pack.GetAssetUUID_Map<aZero::Asset::MeshletMeshData>();
            x = 2;
        }
    }
}