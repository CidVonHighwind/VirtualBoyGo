#version 450

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;
layout(location = 2) in vec2 vSizePx;
layout(location = 3) in float vCornerRadiusPx;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D uImage;

// Same rounded-box SDF as ui_solid.frag - used here to clip an entire
// pre-rendered buffer (e.g. AppMenu's whole composited menu) to rounded
// corners in one draw, rather than rounding each shape inside it
// separately.
float RoundedBoxSDF(vec2 centeredPx, vec2 halfSizePx, float radiusPx) {
    vec2 d = abs(centeredPx) - (halfSizePx - vec2(radiusPx));
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - radiusPx;
}

void main() {
    vec4 texColor = texture(uImage, vUV);
    vec2 localPx = vUV * vSizePx;
    float dist = RoundedBoxSDF(localPx - vSizePx * 0.5, vSizePx * 0.5, vCornerRadiusPx);
    float shapeAlpha = 1.0 - smoothstep(-1.0, 1.0, dist);
    outColor = vec4(texColor.rgb, texColor.a * shapeAlpha);
}
