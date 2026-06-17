#ifndef MATERIAL_GLSL
#define MATERIAL_GLSL

// textureIndex == 0xFFFFFFFF means no texture; shader falls back to per-vertex color.
layout(push_constant) uniform MaterialPC {
    float ambientFactor;
    float diffuseFactor;
    float specularFactor;
    float shininess;
    float alpha;
    uint  textureIndex;
} mat;

#endif // MATERIAL_GLSL
