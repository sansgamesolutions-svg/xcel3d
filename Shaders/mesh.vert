#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragColor;

layout(set = 0, binding = 0) uniform FrameUBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 lightPos;
    float _pad0;
    vec3 lightColor;
    float _pad1;
    vec3 viewPos;
    float _pad2;
} ubo;

void main() {
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    fragPos    = worldPos.xyz;
    fragNormal = mat3(transpose(inverse(ubo.model))) * inNormal;
    fragColor  = inColor;
    gl_Position = ubo.proj * ubo.view * worldPos;
}
