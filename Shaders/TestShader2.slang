#include "TestShader1.slang"

[shader("fragment")]
float4 psMain2(VSOutput input) {
    ShaderData *data = (ShaderData *)pc.DataPtr;
    return float4(textures[0].Sample(float2(textures2[0].Sample(float2(0.f, 0.f)).x, 0.f)).rgb, data->fragment.selected);
}