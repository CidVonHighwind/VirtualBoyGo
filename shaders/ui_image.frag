#version 450

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D uImage;

// Anti-aliased pixel-perfect sampling - clamps the sample position within
// each texel's footprint scaled by the screen-space derivative (fwidth),
// so texel edges stay soft/stable under non-integer scaling or subpixel
// motion (e.g. VR head movement) instead of the harsh shimmer plain
// NEAREST sampling produces there, while still looking crisp/blocky like
// NEAREST when the image is static and scaled by a whole number. Requires
// the sampler to use LINEAR filtering (see UiRenderer::LoadImage).
// Adapted from https://www.shadertoy.com/view/ltfXWS.
vec4 SamplePixelPerfectAA(sampler2D tex, vec2 uv) {
    vec2 texSize = vec2(textureSize(tex, 0));
    vec2 uvTexSpace = uv * texSize;
    vec2 seam = floor(uvTexSpace + 0.5);
    uvTexSpace = (uvTexSpace - seam) / fwidth(uvTexSpace) + seam;
    uvTexSpace = clamp(uvTexSpace, seam - 0.5, seam + 0.5);
    return texture(tex, uvTexSpace / texSize);
}

void main() {
    // Unlike ui_solid.frag/ui_text.frag, no manual gamma decode here: the
    // source image is uploaded as an _SRGB format texture, so the sampler
    // already linearizes on read, and the sRGB swapchain target re-encodes
    // on write - the two conversions cancel out correctly on their own.
    outColor = SamplePixelPerfectAA(uImage, vUV);
    // vColor.a lets callers fade a drawn region (e.g. UiIconSet icons during
    // a page transition) without touching the source pixels' own colour -
    // DrawImage/DrawImageRounded always pass a=1, so this is a no-op there.
    outColor.a *= vColor.a;
    // vColor.rgb lets callers tint a drawn region (e.g. Emulator::DrawScreen's
    // VB color palette) - most callers pass white (1,1,1), a no-op multiply.
    outColor.rgb *= vColor.rgb;
}
