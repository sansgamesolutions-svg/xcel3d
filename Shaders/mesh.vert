#version 450
#extension GL_GOOGLE_include_directive : require

#include "scene_ubo.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

// Per-instance model matrix from binding 1 (VK_VERTEX_INPUT_RATE_INSTANCE).
// mat4 occupies 4 consecutive attribute locations (3-6).
layout(location = 3) in mat4 instModel;

// Per-vertex UV coordinates (location 7, binding 0).
layout(location = 7) in vec2 inTexCoord;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragColor;
layout(location = 3) out vec2 fragTexCoord;

void main() {
    vec4 worldPos  = instModel * vec4(inPosition, 1.0);
    fragPos        = worldPos.xyz;
    fragNormal     = mat3(transpose(inverse(instModel))) * inNormal;
    fragColor      = inColor;
    fragTexCoord   = inTexCoord;
    gl_Position    = ubo.proj * ubo.view * worldPos;
}
