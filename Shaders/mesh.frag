#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

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
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(ubo.lightPos - fragPos);
    vec3 V = normalize(ubo.viewPos - fragPos);
    vec3 H = normalize(L + V);

    float ambient  = 0.15;
    float diffuse  = max(dot(N, L), 0.0);
    float specular = pow(max(dot(N, H), 0.0), 32.0);

    vec3 result = (ambient + diffuse) * fragColor * ubo.lightColor
                + specular * ubo.lightColor * 0.4;

    outColor = vec4(result, 1.0);
}
