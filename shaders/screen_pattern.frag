#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D uImage;

// Full redeclaration matching ui.vert's PushConstants field-for-field
// (only patternColors is actually read here, but a partial
// layout(offset=64)-only redeclaration proved unreliable on Quest's mobile
// GPU driver - see ui.vert's comment on this field). 5 gradient stops
// (darkest to brightest - same left-to-right order as the tint preview's 4
// brightness swatches, see SettingsPage's kBrightnessLevels), owned by the
// C++ side (Settings.cpp's kScreenPatterns) so the same values also drive
// the Color Palette row's preview swatches in SettingsPage - no color data
// duplicated into this shader.
layout(push_constant) uniform PushConstants {
    vec2 posPx;
    vec2 sizePx;
    vec4 uvRect;
    vec4 color;
    vec2 screenSizePx;
    float cornerRadiusPx;
    float pixelScale;
    float patternColors[15];
} pc;

// Identical to ui_image.frag's helper - see its doc comment for why.
vec4 SamplePixelPerfectAA(sampler2D tex, vec2 uv) {
    vec2 texSize = vec2(textureSize(tex, 0));
    vec2 uvTexSpace = uv * texSize;
    vec2 seam = floor(uvTexSpace + 0.5);
    uvTexSpace = (uvTexSpace - seam) / fwidth(uvTexSpace) + seam;
    uvTexSpace = clamp(uvTexSpace, seam - 0.5, seam + 0.5);
    return texture(tex, uvTexSpace / texSize);
}

vec3 Stop(int i) {
    return vec3(pc.patternColors[i * 3], pc.patternColors[i * 3 + 1], pc.patternColors[i * 3 + 2]);
}

// Identical to ui_image.frag's SrgbToLinear - see its doc comment.
vec3 SrgbToLinear(vec3 c) {
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c));
}
vec3 LinearToSrgb(vec3 c) {
    return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(0.0031308, c));
}

void main() {
    vec4 texColor = SamplePixelPerfectAA(uImage, vUV);
    // uImage is _SRGB so texColor is auto-linearized on sample. The gradient
    // thresholds below are tuned against the core's raw 0-255 output value,
    // so re-encode before computing luma to match that calibration.
    vec3 reencoded = LinearToSrgb(texColor.rgb);
    // Emulator::DrawScreen only routes here for the (grayscale) VB screen
    // texture, so any channel is the luminance; dot() is the standard
    // formula and stays correct even if that ever changes.
    float luma = dot(reencoded, vec3(0.2126, 0.7152, 0.0722));

    // 5 stops -> 4 gradient segments, darkest (stop 0) at luma 0.0 up to
    // brightest (stop 4) at luma 1.0. Beetle VB already hands the frontend a
    // continuous 0-255 grayscale value (the VB's real 4 raw brightness
    // levels are remapped through hardware BRTA/BRTB/BRTC registers, which
    // the core bakes into a full grayscale gradient before output - see
    // Emulator::DrawScreen's doc comment), so this reproduces a multi-hue
    // look - inspired by AshleyPikachu's Super Bit Shader
    // (github.com/AshleyPikachu/Super-Bit-Shader), which instead snaps
    // through ~16 hard thresholds - without needing per-pattern threshold
    // calibration.
    float scaled = clamp(luma, 0.0, 1.0) * 4.0;
    int seg = clamp(int(floor(scaled)), 0, 3);
    float f = scaled - float(seg);
    vec3 patternColor = mix(Stop(seg), Stop(seg + 1), f);

    // patternColor is hand-authored in gamma space, same as ui_image.frag's
    // vColor tint - see its doc comment for why this needs the pre-decode.
    // pc.color.a lets callers fade a drawn region (e.g. MenuImage's save-slot
    // preview during a page transition) - see ui_image.frag's vColor.a
    // comment. Emulator::DrawScreen always passes alpha=1, so this is a
    // no-op there.
    outColor = vec4(SrgbToLinear(patternColor), texColor.a * pc.color.a);
}
