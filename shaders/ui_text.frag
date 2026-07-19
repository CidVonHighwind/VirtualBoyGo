#version 450

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D uAtlas;

// Identical to ui_solid.frag's helper - see its doc comment for why.
vec3 SrgbToLinear(vec3 c) {
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c));
}

void main() {
    float a = texture(uAtlas, vUV).r;
    // See ui_solid.frag - vColor is authored in gamma space and needs to be
    // pre-decoded before writing into the sRGB swapchain target.
    outColor = vec4(SrgbToLinear(vColor.rgb), vColor.a * a);
}
