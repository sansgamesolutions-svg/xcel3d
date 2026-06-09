#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

// Per-instance model matrix (binding 1, instanced).
layout(location = 3) in mat4 instModel;

layout(location = 0) out vec3 fragColor;

struct LightGpu {
    vec4 positionAndIntensity;
    vec4 colorAndPad;
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
    vec4 worldPos = instModel * vec4(inPosition, 1.0);
    fragColor     = inColor;
    gl_Position   = ubo.proj * ubo.view * worldPos;
}
