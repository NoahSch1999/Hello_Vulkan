#include "Geo_Structs.slang"

[shader("mesh")]
[NumThreads(g_PrimitivesPerMeshlet, 1, 1)]
[outputtopology("triangle")]
void msMain(
    uint localThreadIndex : SV_GroupIndex,
    uint meshletIndex : SV_GroupID,
    in payload Payload payload,
    out vertices RasterVertex verts[g_VerticesPerMeshlet],
    out indices uint3 tris[g_PrimitivesPerMeshlet]
)
{
    if (Constants.IA_Args.MeshletCount > meshletIndex)
    {
        {
            SetMeshOutputCounts(3, 1);
            tris[localThreadIndex] = uint3(0, 1, 2);
            verts[localThreadIndex].WorldPosition = float3(1, 1, 1);
            verts[localThreadIndex].Position = float4(1, 1, 1, 1);
            verts[localThreadIndex].Normal = float3(1, 1, 1);
            verts[localThreadIndex].UV = float2(1, 1);
            verts[localThreadIndex].Meshletid = 1;
        }
    }

    //if (Constants.IA_Args.MeshletCount > meshletIndex)
    //{
    //    StructuredBuffer<Meshlet> *meshletBuffer = (StructuredBuffer<Meshlet> *)Constants.MeshletBufferAddress;
    //    const uint vertexCount = meshletBuffer.Load(payload.MeshletIndex[meshletIndex]).VertexCount;
    //    {
    //        const uint primitiveCount = meshletBuffer.Load(payload.MeshletIndex[meshletIndex]).PrimitiveCount;
            
    //        SetMeshOutputCounts(vertexCount, primitiveCount);
        
    //        if (localThreadIndex < primitiveCount)
    //        {
    //            tris[localThreadIndex] = Unpack32To8(meshletBuffer.Load(payload.MeshletIndex[meshletIndex]).Primitives[localThreadIndex]);
    //        }
    //    }
    
    //    if (localThreadIndex < vertexCount)
    //    {
    //        StructuredBuffer<MeshVertex> *vertexBuffer = (StructuredBuffer<MeshVertex> *)Constants.VertexBufferAddress;
    //        MeshVertex vertex = vertexBuffer.Load(Constants.IA_Args.GlobalVertexOffset + meshletBuffer.Load(payload.MeshletIndex[meshletIndex]).VertexOffset + localThreadIndex);
    //        float4 positionWorld = mul(Constants.IA_Args.Instance, float4(vertex.Position.xyz, 1.f));
    //        verts[localThreadIndex].WorldPosition = positionWorld.xyz;
    //        CameraData *camera = (CameraData *)Constants.CameraAddress;
    //        positionWorld = mul(camera.ViewProjectionMatrix, positionWorld);
    //        verts[localThreadIndex].Position = positionWorld;
    //        verts[localThreadIndex].Normal = normalize(mul(Constants.IA_Args.Instance, float4(DecodeNormalOctahedral(UnpackOct16(Unpack32To16(vertex.Normal))), 0.f))).xyz;
    //        verts[localThreadIndex].UV = Unpack32ToHalfFloats(vertex.UV);
    //        verts[localThreadIndex].Meshletid = HashColor(meshletIndex);
    //    }
    //}
}