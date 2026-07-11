#version 450

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D uAtlas;

void main() {
    float a = texture(uAtlas, vUV).r;
    // See ui_solid.frag - vColor is authored in gamma space and needs to be
    // pre-decoded before writing into the sRGB swapchain target.
    outColor = vec4(pow(vColor.rgb, vec3(2.2)), vColor.a * a);
}
