#include "Geo_Structs.slang"

//[shader("mesh")]
//[NumThreads(g_PrimitivesPerMeshlet, 1, 1)]
//[outputtopology("triangle")]
//void msMain(
//    uint tid : SV_GroupIndex,
//    uint gid : SV_GroupID,
//    in payload Payload payload,
//    out vertices RasterVertex verts[g_VerticesPerMeshlet],
//    out indices uint3 tris[g_PrimitivesPerMeshlet])
//{
//    SetMeshOutputCounts(3, 1);

//    if (tid == 0)
//    {
//        tris[0] = uint3(0, 1, 2);
//    }

//    if (tid < 3)
//    {
//        // Put three known-good vertices here.

//        float4 poss[3];
//        poss[0] = float4(1, 1, 0, 1);
//        poss[1] = float4(0, 1, 0, 1);
//        poss[2] = float4(0, 0, 0, 1);

//        float4 pos = poss[tid];
//        pos = mul(Constants.IA_Args.Instance, pos);

//        CameraData *camera = (CameraData *)Constants.CameraAddress;
//        pos = mul(camera->ViewProjectionMatrix, pos);

//        verts[tid].Position = pos;

//        verts[tid].WorldPosition = pos.xyz;
//        verts[tid].Normal = pos.xyz;
//        verts[tid].UV = pos.xy;
//        verts[tid].Meshletid = 1;
//    }
//}

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
        Meshlet* meshletBuffer = (Meshlet*)Constants.MeshletBufferAddress;
        const uint vertexCount = meshletBuffer[payload.MeshletIndex[meshletIndex]].VertexCount;
        const uint primitiveCount = meshletBuffer[payload.MeshletIndex[meshletIndex]].PrimitiveCount;
        SetMeshOutputCounts(vertexCount, primitiveCount);

        if (localThreadIndex < primitiveCount)
        {
            tris[localThreadIndex] = Unpack32To8(meshletBuffer[payload.MeshletIndex[meshletIndex]].Primitives[localThreadIndex]);
        }
    
        if (localThreadIndex < vertexCount)
        {
            MeshVertex* vertexBuffer = (MeshVertex*)Constants.VertexBufferAddress;
            MeshVertex vertex = vertexBuffer[Constants.IA_Args.GlobalVertexOffset + meshletBuffer[payload.MeshletIndex[meshletIndex]].VertexOffset + localThreadIndex];
            float4 positionWorld = mul(Constants.IA_Args.Instance, float4(vertex.Position.xyz, 1.f));
            verts[localThreadIndex].WorldPosition = positionWorld.xyz;
            CameraData *camera = (CameraData *)Constants.CameraAddress;
            positionWorld = mul(camera.ViewProjectionMatrix, positionWorld);
            verts[localThreadIndex].Position = positionWorld;
            verts[localThreadIndex].Normal = normalize(mul(Constants.IA_Args.Instance, float4(DecodeNormalOctahedral(UnpackOct16(Unpack32To16(vertex.Normal))), 0.f))).xyz;
            verts[localThreadIndex].UV = Unpack32ToHalfFloats(vertex.UV);
            verts[localThreadIndex].Meshletid = HashColor(meshletIndex);

            verts[localThreadIndex].Color =  float3(  float(localThreadIndex & 1), float((localThreadIndex >> 1) & 1), float((localThreadIndex >> 2) & 1)  );
        }
    }
}