#include "Geo_Structs.slang"

groupshared Payload payload;

[shader("task")]
[NumThreads(TREADS_PER_WAVE, 1, 1)]
void asMain(uint3 dtid : SV_DispatchThreadID, uint3 gid : SV_GroupThreadID)
{
    bool visible = false;
    if (dtid.x < Constants->IA_Args.MeshletCount)
    {
        StructuredBuffer<BoundingSphere> *meshletBoundsBuffer = (StructuredBuffer<BoundingSphere> *)Constants.MeshletBoundBufferAddress;
        CameraData *camera = (CameraData *)Constants.CameraAddress;
        BoundingSphere meshletBounds = meshletBoundsBuffer.Load(Constants->IA_Args.GlobalMeshletOffset + dtid.x);
        // visible = camera.Frustum.Intersects( CreateBoundingSphere(mul(Constants->IA_Args.Instance, float4(meshletBounds.Position, 1.f)).xyz, meshletBounds.Radius), camera.ViewMatrix);
        visible = true;
        if (visible)
        {
            payload.MeshletIndex[WavePrefixCountBits(visible)] = Constants->IA_Args.GlobalMeshletOffset + dtid.x;
        }
    }
    
    DispatchMesh(WaveActiveCountBits(visible), 1, 1, payload);
}