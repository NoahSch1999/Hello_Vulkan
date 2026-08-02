#include "shader.slang"

#ifdef HELLO
[[vk::binding(1, 1)]]
SamplerState ssss[];
#endif

[shader("fragment")]
float4 psMain(VSOutput input) {
    ShaderData *shaderData = (ShaderData *)pc.address;
    return float4(textures[0].Sample(input.UV).rgb, shaderData->selected);
    //return float4((input.Normal + 1) * 0.5, 1.0);
    //return float4(input.UV, 0.0, 1.0);
}