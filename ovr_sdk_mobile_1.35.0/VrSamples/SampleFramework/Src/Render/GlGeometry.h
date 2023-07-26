/************************************************************************************

Filename    :   GlGeometry.h
Content     :   OpenGL geometry setup.
Created     :   October 8, 2013
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#pragma once

#include <vector>
#include "OVR_Math.h"

#include "GlProgram.h"

namespace OVRFW {

struct VertexAttribs {
    std::vector<OVR::Vector3f> position;
    std::vector<OVR::Vector3f> normal;
    std::vector<OVR::Vector3f> tangent;
    std::vector<OVR::Vector3f> binormal;
    std::vector<OVR::Vector4f> color;
    std::vector<OVR::Vector2f> uv0;
    std::vector<OVR::Vector2f> uv1;
    std::vector<OVR::Vector4i> jointIndices;
    std::vector<OVR::Vector4f> jointWeights;
};

typedef uint16_t TriangleIndex;

class GlGeometry {
   public:
    GlGeometry()
        : vertexBuffer(0),
          indexBuffer(0),
          vertexArrayObject(0),
          primitiveType(0x0004 /* GL_TRIANGLES */),
          vertexCount(0),
          indexCount(0),
          localBounds(OVR::Bounds3f::Init) {}

    GlGeometry(const VertexAttribs& attribs, const std::vector<TriangleIndex>& indices)
        : vertexBuffer(0),
          indexBuffer(0),
          vertexArrayObject(0),
          primitiveType(0x0004 /* GL_TRIANGLES */),
          vertexCount(0),
          indexCount(0),
          localBounds(OVR::Bounds3f::Init) {
        Create(attribs, indices);
    }

    // Create the VAO and vertex and index buffers from arrays of data.
    void Create(const VertexAttribs& attribs, const std::vector<TriangleIndex>& indices);
    void Update(const VertexAttribs& attribs, const bool updateBounds = true);

    // Free the buffers and VAO, assuming that they are strictly for this geometry.
    // We could save some overhead by packing an entire model into a single buffer, but
    // it would add more coupling to the structures.
    // This is not in the destructor to allow objects of this class to be passed by value.
    void Free();

   public:
    static constexpr int32_t MAX_GEOMETRY_VERTICES = 1 << (sizeof(TriangleIndex) * 8);
    static constexpr int32_t MAX_GEOMETRY_INDICES = 1024 * 1024 * 3;

    static constexpr inline int32_t GetMaxGeometryVertices() {
        return MAX_GEOMETRY_VERTICES;
    }
    static constexpr inline int32_t GetMaxGeometryIndices() {
        return MAX_GEOMETRY_INDICES;
    }

    static unsigned IndexType; // GL_UNSIGNED_SHORT, GL_UNSIGNED_INT, etc.

   public:
    unsigned vertexBuffer;
    unsigned indexBuffer;
    unsigned vertexArrayObject;
    unsigned primitiveType; // GL_TRIANGLES / GL_LINES / GL_POINTS / etc
    int vertexCount;
    int indexCount;
    OVR::Bounds3f localBounds;
};

// Build it in a -1 to 1 range, which will be scaled to the appropriate
// aspect ratio for each usage.
//
// A horizontal and vertical value of 1 will give a single quad.
//
// Texcoords range from 0 to 1.
//
// Color is 1, fades alpha to 0 along the outer edge.
GlGeometry BuildTesselatedQuad(
    const TriangleIndex horizontal,
    const TriangleIndex vertical,
    bool twoSided = false);

// 8 quads making a thin border inside the -1 tp 1 square.
// The fractions are the total fraction that will be faded,
// half on one side, half on the other.
GlGeometry BuildVignette(const float xFraction, const float yFraction);

// Build it in a -1 to 1 range, which will be scaled to the appropriate
// aspect ratio for each usage.
// Fades alpha to 0 along the outer edge.
GlGeometry BuildTesselatedCylinder(
    const float radius,
    const float height,
    const TriangleIndex horizontal,
    const TriangleIndex vertical,
    const float uScale,
    const float vScale);

GlGeometry BuildTesselatedCapsule(
    const float radius,
    const float height,
    const TriangleIndex horizontal,
    const TriangleIndex vertical);

GlGeometry BuildDome(const float latRads, const float uScale = 1.0f, const float vScale = 1.0f);
GlGeometry
BuildGlobe(const float uScale = 1.0f, const float vScale = 1.0f, const float radius = 100.0f);

// Make a square patch on a sphere that can rotate with the viewer
// so it always covers the screen.
GlGeometry BuildSpherePatch(const float fov);

// 12 edges of a 0 to 1 unit cube.
GlGeometry BuildUnitCubeLines();

// 1.0 width cube, centered around the 0,0,0 point
GlGeometry BuildUnitCube(float side = 0.5f);

GlGeometry BuildAxis(float sideLength = 0.1f, float sideRatio = 0.25f);

} // namespace OVRFW
