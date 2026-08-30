#include "Geo_Draw_Structs.slang"

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
    if (payload.MeshletCount[meshletIndex] > meshletIndex)
    {
        InstanceData *instanceDataBuffer = (InstanceData *)Constants.InstanceDataBufferPtr;
        InstanceData instanceData = instanceDataBuffer[payload.InstanceIndex[meshletIndex]];

        Meshlet *meshletBuffer = (Meshlet *)instanceData.MeshletBDA;
        const uint vertexCount = meshletBuffer[payload.MeshletIndex[meshletIndex]].VertexCount;
        const uint primitiveCount = meshletBuffer[payload.MeshletIndex[meshletIndex]].PrimitiveCount;
        SetMeshOutputCounts(vertexCount, primitiveCount);

        if (localThreadIndex < primitiveCount)
        {
            tris[localThreadIndex] = Unpack32To8(meshletBuffer[payload.MeshletIndex[meshletIndex]].Primitives[localThreadIndex]);
        }
    
        if (localThreadIndex < vertexCount)
        {
            MeshVertex *vertexBuffer = (MeshVertex *)instanceData.VertexBDA;
            MeshVertex vertex = vertexBuffer[meshletBuffer[payload.MeshletIndex[meshletIndex]].VertexOffset + localThreadIndex];
            float4 positionWorld = mul(instanceData.WorldMatrix, float4(vertex.Position.xyz, 1.f));
            verts[localThreadIndex].WorldPosition = positionWorld.xyz;
            CameraData *camera = (CameraData *)Constants.CameraBufferPtr;
            positionWorld = mul(camera.ViewProjectionMatrix, positionWorld);
            verts[localThreadIndex].Position = positionWorld;
            verts[localThreadIndex].Normal = normalize(mul(instanceData.WorldMatrix, float4(DecodeNormalOctahedral(UnpackOct16(Unpack32To16(vertex.Normal))), 0.f))).xyz;
            verts[localThreadIndex].UV = Unpack32ToHalfFloats(vertex.UV);
            verts[localThreadIndex].MaterialIndex = instanceData.MaterialIndex;
            verts[localThreadIndex].Meshletid = HashColor(meshletIndex);
        }
    }
}