#include "Geo_Pipeline_Structs.slang"

struct IndirectArguments
{
    uint32_t GroupX;
    uint32_t GroupY;
    uint32_t GroupZ;
};

struct IndirectArgumentCounter
{
    uint32_t Count;
    uint32_t Pad[3];
};

[[vk::push_constant]]
ConstantBuffer<InstanceCullPushConstants> Constants;

[numthreads(THREAD_COUNT_X, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    if (dtid.x < Constants.InstanceCount)
    {
        InstanceCullData *instanceCullDataBuffer = (InstanceCullData *)Constants.InstanceCullDataBufferPtr;
        const InstanceCullData instanceCullData = instanceCullDataBuffer[dtid.x];

        CameraData *camera = (CameraData *)Constants.CameraBufferPtr;
        // if (camera.Frustum.Intersects(instanceCullData.WorldSpaceBounds, camera.ViewMatrix))
        {
            IndirectArgumentCounter *indirectArgumentCounterBuffer = (IndirectArgumentCounter *)Constants.IndirectArgumentBufferPtr;
            IndirectArguments *indirectArgumentsBuffer = (IndirectArguments *)(Constants.IndirectArgumentBufferPtr + sizeof(IndirectArgumentCounter));

            uint meshInstanceIndex;
            InterlockedAdd(indirectArgumentCounterBuffer[0].Count, 1, meshInstanceIndex);
            indirectArgumentsBuffer[meshInstanceIndex].GroupX = (uint32_t)(ceil(instanceCullData.MeshletCount / (float) THREAD_COUNT_X));
            indirectArgumentsBuffer[meshInstanceIndex].GroupY = 1;
            indirectArgumentsBuffer[meshInstanceIndex].GroupZ = 1;

            DrawMetaData *drawDataBuffer = (DrawMetaData *)Constants.DrawMetadataBufferPtr;
            drawDataBuffer[meshInstanceIndex].InstanceIndex = dtid.x;
            drawDataBuffer[meshInstanceIndex].MeshletCount = instanceCullData.MeshletCount;
        }
    }
}