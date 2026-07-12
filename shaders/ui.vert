#version 450

// A single shared unit quad (0,0)-(1,1) is bound as the vertex buffer for
// every UI draw call (solid rects and per-glyph text quads alike); the
// actual position/size/UV-rect come from push constants.
layout(location = 0) in vec2 inUnitPos;

layout(push_constant) uniform PushConstants {
    vec2 posPx;
    vec2 sizePx;
    vec4 uvRect;      // xMin, yMin, xMax, yMax - unused (0,0,1,1) for solid quads
    vec4 color;
    vec2 screenSizePx;
    float cornerRadiusPx;  // only read by ui_solid.frag - 0 for a plain rect
    float pixelScale;      // physical pixels per logical unit - see PushConstants comment
} pc;

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;
layout(location = 2) out vec2 vSizePx;
layout(location = 3) out float vCornerRadiusPx;
layout(location = 4) out float vPixelScale;

void main() {
    vec2 posPx = pc.posPx + inUnitPos * pc.sizePx;
    vec2 ndc = (posPx / pc.screenSizePx) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    vUV = mix(pc.uvRect.xy, pc.uvRect.zw, inUnitPos);
    vColor = pc.color;
    vSizePx = pc.sizePx;
    vCornerRadiusPx = pc.cornerRadiusPx;
    vPixelScale = pc.pixelScale;
}
