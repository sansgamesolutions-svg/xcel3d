#version 450

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inAccum;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput inReveal;

layout(location = 0) out vec4 outColor;

void main() {
    vec4  acc     = subpassLoad(inAccum);
    float reveal  = subpassLoad(inReveal).r;
    vec3  average = acc.rgb / max(acc.a, 1e-5);
    outColor = vec4(average, 1.0 - reveal);
}
