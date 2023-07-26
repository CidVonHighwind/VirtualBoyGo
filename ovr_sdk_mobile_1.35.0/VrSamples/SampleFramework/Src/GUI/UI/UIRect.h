/************************************************************************************

Filename    :   UIRect.h
Content     :
Created     :	2/17/2015
Authors     :   Jim Dose

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#pragma once

#include "VrApi_Types.h"

namespace OVRFW {

enum RectPosition {
    TOP_LEFT,
    TOP,
    TOP_RIGHT,
    LEFT,
    CENTER,
    RIGHT,
    BOTTOM_LEFT,
    BOTTOM,
    BOTTOM_RIGHT
};

//-----------------------------------------------------------------------------------
// ***** UIRect

// Rect describes a rectangular area for rendering

template <class T>
class UIRect {
   public:
    T Left, Bottom, Right, Top;

    UIRect() : Left(0), Bottom(0), Right(0), Top(0) {}
    UIRect(T value) : Left(value), Bottom(value), Right(value), Top(value) {}
    UIRect(T left, T bottom, T right, T top) : Left(left), Bottom(bottom), Right(right), Top(top) {}

    OVR::Vector2<T> GetCenter() const {
        return OVR::Vector2<T>((Left + Right) * 0.5f, (Top + Bottom) * 0.5f);
    }
    OVR::Size<T> GetSize() const {
        return OVR::Size<T>(Right - Left, Top - Bottom);
    }
    T GetWidth() const {
        return Right - Left;
    }
    T GetHeight() const {
        return Bottom - Top;
    }

    bool operator==(const UIRect& r) const {
        return (Left == r.Left) && (Top == r.Top) && (Right == r.Right) && (Bottom == r.Bottom);
    }
    bool operator!=(const UIRect& r) const {
        return !operator==(r);
    }

    UIRect operator*(T s) const {
        return UIRect(Left * s, Bottom * s, Right * s, Top * s);
    }
    UIRect& operator*=(T s) {
        Left *= s;
        Bottom *= s;
        Right *= s;
        Top *= s;
        return *this;
    }
    UIRect operator/(T s) const {
        return UIRect(Left * s, Bottom * s, Right * s, Top * s);
    }
    UIRect& operator/=(T s) {
        Left /= s;
        Bottom /= s;
        Right /= s;
        Top /= s;
        return *this;
    }

    void ExpandBy(const UIRect& r) {
        Left -= r.Left;
        Bottom -= r.Bottom;
        Right += r.Right;
        Top += r.Top;
    }

    void ShrinkBy(const UIRect& r) {
        Left += r.Left;
        Bottom += r.Bottom;
        Right -= r.Right;
        Top -= r.Top;
    }

    OVR::Vector3f GetPosition(RectPosition position) const {
        switch (position) {
            case TOP_LEFT:
                return OVR::Vector3f(Left, Top, 0.0f);
                break;

            case TOP:
                return OVR::Vector3f((Left + Right) * 0.5f, Top, 0.0f);
                break;

            case TOP_RIGHT:
                return OVR::Vector3f(Right, Top, 0.0f);
                break;

            case LEFT:
                return OVR::Vector3f(Left, (Top + Bottom) * 0.5f, 0.0f);
                break;

            default:
            case CENTER:
                return OVR::Vector3f((Left + Right) * 0.5f, (Top + Bottom) * 0.5f, 0.0f);
                break;

            case RIGHT:
                return OVR::Vector3f(Right, (Top + Bottom) * 0.5f, 0.0f);
                break;

            case BOTTOM_LEFT:
                return OVR::Vector3f(Left, Bottom, 0.0f);
                break;

            case BOTTOM:
                return OVR::Vector3f((Left + Right) * 0.5f, Bottom, 0.0f);
                break;

            case BOTTOM_RIGHT:
                return OVR::Vector3f(Right, Bottom, 0.0f);
                break;
        }
    }
};

// allow multiplication in order scalar * UIRect (member operator handles UIRect * scalar)
template <class T>
UIRect<T> operator*(T s, const UIRect<T>& r) {
    return UIRect<T>(r.Left * s, r.Bottom * s, r.Right * s, r.Top * s);
}

typedef UIRect<int> UIRecti;
typedef UIRect<float> UIRectf;

} // namespace OVRFW
