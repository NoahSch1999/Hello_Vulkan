struct VSInput {
    float3 Pos;
    uint Normal;
    uint UV;
};

Sampler2D textures[];

struct ShaderData {
    float4x4 projection;
    float4x4 view;
    float4x4 model[3];
    float4 lightPos;
    uint32_t selected;
};

struct VSOutput {
    float4 Pos : SV_POSITION;
    float3 Normal;
    float2 UV;
};

float2 UnpackOct16(uint2 p)
{
    float2 n;
    n.x = (p.x / 65535.0) * 2.0 - 1.0;
    n.y = (p.y / 65535.0) * 2.0 - 1.0;
    return n;
}

float3 DecodeNormalOctahedral(float2 e)
{
    float3 n = float3(e.x, e.y, 1.0 - abs(e.x) - abs(e.y));

    if (n.z < 0.0)
    {
        float2 signNotZero = float2(
            n.x >= 0.0 ? 1.0 : -1.0,
            n.y >= 0.0 ? 1.0 : -1.0
        );

        n.xy = (1.0 - abs(n.yx)) * signNotZero;
    }

    return normalize(n);
}

uint2 Unpack32To16(uint input)
{
    uint ch1, ch2;
    ch1 = (input & 0x0000ffff);
    ch2 = (input & 0xffff0000) >> 16;
    return uint2(ch1, ch2);
}

float2 Unpack32ToHalfFloats(uint p)
{
    return float2(f16tof32(p & 0xFFFF), f16tof32(p >> 16));
}

[shader("vertex")]
VSOutput main(VSInput input, uniform ShaderData *shaderData, uint instanceIndex : SV_VulkanInstanceID) {
    VSOutput output;
    float4x4 modelMat = shaderData->model[instanceIndex];
    output.Normal = normalize(mul((float3x3)modelMat, DecodeNormalOctahedral(UnpackOct16(Unpack32To16(input.Normal)))));
    output.UV = Unpack32ToHalfFloats(input.UV);
    output.Pos = mul(shaderData->projection, mul(shaderData->view, mul(modelMat, float4(input.Pos.x, input.Pos.y * -1.0, input.Pos.z, 1.0))));

    return output;
}

[shader("fragment")]
float4 main(VSOutput input) {
    return float4(textures[0].Sample(input.UV).rgb, 1.0);
    //return float4((input.Normal + 1) * 0.5, 1.0);
    //return float4(input.UV, 0.0, 1.0);
}