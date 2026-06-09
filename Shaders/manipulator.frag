#version 450

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

// xyz = solid color override; w = alpha.
// When w == 0.0 the per-vertex color (fragColor) is used at full opacity.
layout(push_constant) uniform ManipulatorPC {
    vec4 colorOverride;
} pc;

void main() {
    if (pc.colorOverride.w > 0.0)
        outColor = pc.colorOverride;
    else
        outColor = vec4(fragColor, 1.0);
}
