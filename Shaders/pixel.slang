#include "shader.slang"

[shader("fragment")]
float4 psMain(VSOutput input) {
    return float4(textures[0].Sample(input.UV));
}