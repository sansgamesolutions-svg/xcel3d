#ifndef SCENE_UBO_GLSL
#define SCENE_UBO_GLSL

#include "lighting.glsl"

layout(set = 0, binding = 0) uniform FrameUBO {
    mat4     model;
    mat4     view;
    mat4     proj;
    vec3     viewPos;
    uint     lightCount;
    LightGpu lights[8];
    vec4     sectionPlane; // xyz=world-normal, w=d; length(xyz)==0 means disabled
} ubo;

#endif // SCENE_UBO_GLSL
