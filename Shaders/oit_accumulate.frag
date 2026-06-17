#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "scene_ubo.glsl"
#include "bindless_textures.glsl"
#include "material.glsl"

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor;
layout(location = 3) in vec2 fragTexCoord;

// Weighted-blended OIT accumulation targets (McGuire & Bavoil, JCGT 2013).
layout(location = 0) out vec4  outAccum;
layout(location = 1) out float outReveal;

void main() {
    if (dot(ubo.sectionPlane.xyz, ubo.sectionPlane.xyz) > 0.01 &&
        dot(fragPos, ubo.sectionPlane.xyz) + ubo.sectionPlane.w < 0.0)
        discard;

    vec3 albedo = fragColor;
    if (mat.textureIndex != 0xFFFFFFFFu)
        albedo = texture(sampler2D(textures[nonuniformEXT(mat.textureIndex)], texSampler),
                         fragTexCoord).rgb;

    vec3 N = normalize(fragNormal);
    vec3 V = normalize(ubo.viewPos - fragPos);

    vec3 shaded = BlinnPhong(albedo, N, V, fragPos, ubo.lightCount, ubo.lights,
                              mat.ambientFactor, mat.diffuseFactor,
                              mat.specularFactor, mat.shininess);

    float a = mat.alpha;

    // Depth-weighted contribution: fragments closer to the camera dominate the
    // average. Branchless form of the weighting function from the paper.
    float w = a * max(1e-2, 3e3 * pow(1.0 - gl_FragCoord.z * 0.99, 3.0));

    outAccum  = vec4(shaded * a, a) * w;
    outReveal = a;
}
