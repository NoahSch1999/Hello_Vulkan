#include "shader.slang"

#ifdef HELLO
[[vk::binding(1, 1)]]
SamplerState ssss[];
#endif

//struct Idk
//{
//    [[vk::offset(16)]]
//    float idk;
//};

//[[vk::push_constant]]
//ConstantBuffer<Idk> pcFragment;

[shader("fragment")]
float4 psMain(VSOutput input) {
    return float4(textures[0].Sample(input.UV) * pc.idk);
    //return float4((input.Normal + 1) * 0.5, 1.0);
    //return float4(input.UV, 0.0, 1.0);
}