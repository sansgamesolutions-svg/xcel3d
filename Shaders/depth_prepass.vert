#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;   // unused, keeps binding compatible with mesh.vert
layout(location = 2) in vec3 inColor;    // unused

layout(location = 3) in mat4 instModel;

layout(set = 0, binding = 0) uniform FrameUBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 lightPos;   float _pad0;
    vec3 lightColor; float _pad1;
    vec3 viewPos;    float _pad2;
} ubo;

void main() {
    gl_Position = ubo.proj * ubo.view * instModel * vec4(inPosition, 1.0);
}
