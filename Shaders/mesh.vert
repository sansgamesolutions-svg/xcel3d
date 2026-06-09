#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

// Per-instance model matrix from binding 1 (VK_VERTEX_INPUT_RATE_INSTANCE).
// mat4 occupies 4 consecutive attribute locations (3-6).
layout(location = 3) in mat4 instModel;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragColor;

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
    vec4     sectionPlane;
} ubo;

void main() {
    vec4 worldPos  = instModel * vec4(inPosition, 1.0);
    fragPos        = worldPos.xyz;
    fragNormal     = mat3(transpose(inverse(instModel))) * inNormal;
    fragColor      = inColor;
    gl_Position    = ubo.proj * ubo.view * worldPos;
}
