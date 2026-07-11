#pragma once

// Minimal column-major 4x4 matrix math for OpenXR poses/FOV -> Vulkan clip
// space. Deliberately small and self-written (not pulled from
// OpenXR-SDK-Source's common/xr_linear.h) so this project has no dependency
// on that sample tree.

#include <openxr/openxr.h>

#include <cmath>

struct Mat4 {
    float m[16]{};  // column-major: m[col * 4 + row]

    static Mat4 Identity() {
        Mat4 r;
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }
};

inline Mat4 Mat4Multiply(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            r.m[col * 4 + row] = sum;
        }
    }
    return r;
}

// Vulkan clip space (NDC depth [0,1], Y-down) projection from an OpenXR FOV.
inline Mat4 Mat4ProjectionVulkan(const XrFovf& fov, float nearZ, float farZ) {
    const float tanLeft = tanf(fov.angleLeft);
    const float tanRight = tanf(fov.angleRight);
    const float tanUp = tanf(fov.angleUp);
    const float tanDown = tanf(fov.angleDown);

    const float tanWidth = tanRight - tanLeft;
    const float tanHeight = tanUp - tanDown;

    Mat4 r;
    r.m[0] = 2.0f / tanWidth;
    r.m[5] = -2.0f / tanHeight;
    r.m[8] = (tanRight + tanLeft) / tanWidth;
    r.m[9] = -(tanUp + tanDown) / tanHeight;
    r.m[10] = farZ / (nearZ - farZ);
    r.m[11] = -1.0f;
    r.m[14] = (farZ * nearZ) / (nearZ - farZ);
    return r;
}

inline Mat4 Mat4FromQuatTranslation(const XrQuaternionf& q, const XrVector3f& t) {
    Mat4 r = Mat4::Identity();
    const float x = q.x, y = q.y, z = q.z, w = q.w;
    r.m[0] = 1 - 2 * (y * y + z * z);
    r.m[1] = 2 * (x * y + z * w);
    r.m[2] = 2 * (x * z - y * w);
    r.m[4] = 2 * (x * y - z * w);
    r.m[5] = 1 - 2 * (x * x + z * z);
    r.m[6] = 2 * (y * z + x * w);
    r.m[8] = 2 * (x * z + y * w);
    r.m[9] = 2 * (y * z - x * w);
    r.m[10] = 1 - 2 * (x * x + y * y);
    r.m[12] = t.x;
    r.m[13] = t.y;
    r.m[14] = t.z;
    return r;
}

// Inverse of a rigid (rotation + translation, no scale) transform.
inline Mat4 Mat4InvertRigid(const Mat4& src) {
    Mat4 r = Mat4::Identity();
    r.m[0] = src.m[0];
    r.m[1] = src.m[4];
    r.m[2] = src.m[8];
    r.m[4] = src.m[1];
    r.m[5] = src.m[5];
    r.m[6] = src.m[9];
    r.m[8] = src.m[2];
    r.m[9] = src.m[6];
    r.m[10] = src.m[10];
    const float tx = src.m[12], ty = src.m[13], tz = src.m[14];
    r.m[12] = -(r.m[0] * tx + r.m[4] * ty + r.m[8] * tz);
    r.m[13] = -(r.m[1] * tx + r.m[5] * ty + r.m[9] * tz);
    r.m[14] = -(r.m[2] * tx + r.m[6] * ty + r.m[10] * tz);
    return r;
}

inline Mat4 Mat4Scale(const XrVector3f& s) {
    Mat4 r = Mat4::Identity();
    r.m[0] = s.x;
    r.m[5] = s.y;
    r.m[10] = s.z;
    return r;
}
