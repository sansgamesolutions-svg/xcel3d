#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

struct LightGpu {
    vec4 positionAndIntensity; // xyz = world position, w = intensity
    vec4 colorAndPad;          // xyz = linear RGB color, w = 0
};

// Blinn-Phong accumulation over a fixed-size light array (size matches FrameUBO.lights).
vec3 BlinnPhong(
    vec3      albedo,
    vec3      N,
    vec3      V,
    vec3      fragPos,
    uint      lightCount,
    LightGpu  lights[8],
    float     ambientFactor,
    float     diffuseFactor,
    float     specularFactor,
    float     shininess)
{
    vec3 result = ambientFactor * albedo;

    for (uint i = 0u; i < lightCount; ++i) {
        vec3  lPos   = lights[i].positionAndIntensity.xyz;
        float lInten = lights[i].positionAndIntensity.w;
        vec3  lColor = lights[i].colorAndPad.xyz;

        vec3  L    = normalize(lPos - fragPos);
        vec3  H    = normalize(L + V);
        float diff = max(dot(N, L), 0.0);
        float spec = pow(max(dot(N, H), 0.0), shininess);

        result += (diff * diffuseFactor * albedo
                 + spec * specularFactor) * lColor * lInten;
    }

    return result;
}

#endif // LIGHTING_GLSL
