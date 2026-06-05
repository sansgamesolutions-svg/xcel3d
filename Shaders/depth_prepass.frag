#version 450

layout(location = 0) out float outDepth;

void main() {
    // Write NDC depth to R32_SFLOAT color attachment for Hi-Z pyramid.
    outDepth = gl_FragCoord.z;
}
