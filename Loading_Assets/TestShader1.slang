
struct VSInput {
    float3 Pos;
    uint Normal;
    uint UV;
};

[[vk::binding(1, 0)]]
Sampler2D textures[];

[[vk::binding(0, 0)]]
Sampler2D textures2[3];

struct VertexData
{
    float4x4 projection;
    float4x4 view;
    float4x4 model[3];
};

struct FragmentData
{
    uint32_t selected;
};

struct ShaderData
{
    VertexData vertex;
    FragmentData fragment;
};

struct PushConstants
{
    uint64_t DataPtr;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> pc;

struct VSOutput {
    float4 Pos : SV_POSITION;
};

[shader("vertex")]
VSOutput vsMain2(VSInput input, uint instanceIndex : SV_VulkanInstanceID) {
    VSOutput output;
    ShaderData* data = (ShaderData*)pc.DataPtr;
    float4x4 modelMat = data->vertex.model[instanceIndex];
    output.Pos = mul(data->vertex.projection, mul(data->vertex.view, mul(modelMat, float4(input.Pos.x, input.Pos.y * -1.0, input.Pos.z, 1.0))));
    return output;
}