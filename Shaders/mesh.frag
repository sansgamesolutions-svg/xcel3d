#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

struct LightGpu {
    vec4 positionAndIntensity; // xyz = world position, w = intensity
    vec4 colorAndPad;          // xyz = linear RGB color, w = 0
};

layout(set = 0, binding = 0) uniform FrameUBO {
    mat4     model;
    mat4     view;
    mat4     proj;
    vec3     viewPos;
    uint     lightCount;
    LightGpu lights[8];
    vec4     sectionPlane; // xyz=world-normal, w=d; length(xyz)==0 means disabled
} ubo;

layout(push_constant) uniform MaterialPC {
    float ambientFactor;
    float diffuseFactor;
    float specularFactor;
    float shininess;
} mat;

void main() {
    if (dot(ubo.sectionPlane.xyz, ubo.sectionPlane.xyz) > 0.01 &&
        dot(fragPos, ubo.sectionPlane.xyz) + ubo.sectionPlane.w < 0.0)
        discard;

    vec3 N = normalize(fragNormal);
    vec3 V = normalize(ubo.viewPos - fragPos);

    vec3 result = mat.ambientFactor * fragColor;

    for (uint i = 0u; i < ubo.lightCount; ++i) {
        vec3  lPos   = ubo.lights[i].positionAndIntensity.xyz;
        float lInten = ubo.lights[i].positionAndIntensity.w;
        vec3  lColor = ubo.lights[i].colorAndPad.xyz;

        vec3  L    = normalize(lPos - fragPos);
        vec3  H    = normalize(L + V);
        float diff = max(dot(N, L), 0.0);
        float spec = pow(max(dot(N, H), 0.0), mat.shininess);

        result += (diff * mat.diffuseFactor * fragColor
                 + spec * mat.specularFactor) * lColor * lInten;
    }

    outColor = vec4(result, 1.0);
}
