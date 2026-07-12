#version 450

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;
layout(location = 2) in vec2 vSizePx;
layout(location = 3) in float vCornerRadiusPx;
layout(location = 4) in float vPixelScale;
layout(location = 0) out vec4 outColor;

// Rounded-box SDF (Inigo Quilez's formula) - degenerates to a plain
// rectangle when vCornerRadiusPx is 0, so this single shader covers both
// sharp- and rounded-corner quads with no extra pipeline.
float RoundedBoxSDF(vec2 centeredPx, vec2 halfSizePx, float radiusPx) {
    vec2 d = abs(centeredPx) - (halfSizePx - vec2(radiusPx));
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - radiusPx;
}

void main() {
    float shapeAlpha;
    if (vCornerRadiusPx <= 0.0) {
        // No rounding - output pixel-perfect, no SDF or edge falloff.
        shapeAlpha = 1.0;
    } else {
        vec2 localPx = vUV * vSizePx;
        float dist = RoundedBoxSDF(localPx - vSizePx * 0.5, vSizePx * 0.5, vCornerRadiusPx);
        // ~1 physical pixel antialiased edge falloff - dist is in logical
        // units, so the band's logical-unit width has to shrink as
        // vPixelScale grows to stay 1 physical pixel wide (see PushConstants'
        // pixelScale comment).
        float aaWidth = 1.0 / vPixelScale;
        shapeAlpha = 1.0 - smoothstep(-aaWidth, aaWidth, dist);
    }

    // vColor is authored in gamma (sRGB) space, but this pipeline renders
    // into an sRGB swapchain format, which makes the hardware apply its own
    // linear->sRGB encode on store. Pre-decode here so the two conversions
    // cancel out and the stored value matches what was authored (otherwise
    // colors wash out/fade - the same double-gamma issue the composition
    // layer's test image had on the sampling side).
    outColor = vec4(pow(vColor.rgb, vec3(2.2)), vColor.a * shapeAlpha);
}
