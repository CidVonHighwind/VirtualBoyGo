#version 450

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;
layout(location = 2) in vec2 vSizePx;
layout(location = 3) in float vCornerRadiusPx;
layout(location = 4) in float vPixelScale;
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
    // See ui_solid.frag - dist is in logical units, so the ~1-physical-pixel
    // AA band has to be scaled by vPixelScale to stay 1 physical pixel wide.
    float aaWidth = 1.0 / vPixelScale;
    float shapeAlpha = 1.0 - smoothstep(-aaWidth, aaWidth, dist);
    outColor = vec4(texColor.rgb, texColor.a * shapeAlpha);
}
