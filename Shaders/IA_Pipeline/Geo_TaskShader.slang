#include "Geo_Draw_Structs.slang"

groupshared Payload payload;

[shader("task")]
[NumThreads(THREAD_COUNT_X, 1, 1)]
void asMain(uint3 dtid: SV_DispatchThreadID, uint3 gid: SV_GroupThreadID, uint32_t drawIndex : SV_DrawIndex)
{
    uint32_t drawid = drawIndex;
    DrawMetaData *drawDataBuffer = (DrawMetaData *)Constants.DrawMetadataBufferPtr;
    DrawMetaData drawData = drawDataBuffer[drawid];

    bool visible = false;

    if (dtid.x < drawData.MeshletCount)
    {
        BoundingSphere *meshletBoundsBuffer = (BoundingSphere *)Constants.MeshletBoundBufferPtr;
        CameraData *camera = (CameraData *)Constants.CameraBufferPtr;
        InstanceData *instanceDataBuffer = (InstanceData *)Constants.InstanceDataBufferPtr;
        InstanceData instanceData = instanceDataBuffer[drawData.InstanceIndex];

        BoundingSphere meshletBounds = meshletBoundsBuffer[drawData.InstanceIndex + dtid.x];
        // visible = camera.Frustum.Intersects( CreateBoundingSphere(mul(instanceData.WorldMatrix, float4(meshletBounds.Position, 1.f)).xyz, meshletBounds.Radius), camera->ViewMatrix);
        visible = true;
        if (visible)
        {
            uint index = WavePrefixCountBits(visible);

            payload.MeshletIndex[index] = dtid.x;
            payload.InstanceIndex = drawData.InstanceIndex;
            payload.MeshletCount = drawData.MeshletCount;
        }
    }
    
    DispatchMesh(WaveActiveCountBits(visible), 1, 1, payload);
}