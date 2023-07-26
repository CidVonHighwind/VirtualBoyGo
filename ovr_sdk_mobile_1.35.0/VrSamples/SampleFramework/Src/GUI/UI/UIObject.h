/************************************************************************************

Filename    :   UIObject.h
Content     :
Created     :	1/5/2015
Authors     :   Jim Dose

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#pragma once

#include "GUI/VRMenu.h"
#include "GUI/GuiSys.h"
#include "UIRect.h"

namespace OVRFW {

class VrAppInterface;
class UIMenu;
class UITexture;
class VRMenuObject;

class UIObject {
   public:
    UIObject(OvrGuiSys& guiSys);
    virtual ~UIObject();

    OvrGuiSys& GetGuiSys() const {
        return GuiSys;
    }

    void
    SetMenuObject(UIMenu* menu, VRMenuObject* vrMenuObject, UIObject* vrMenuObjectParent = NULL);

    size_t GetNumChildren() const {
        return static_cast<size_t>(Children.size());
    }
    UIObject* GetChildForIndex(size_t index) const {
        return Children[index];
    }
    const std::vector<UIObject*>& GetChildren() const {
        return Children;
    }
    void RemoveChild(UIObject* child);

    VRMenuId_t GetId() const {
        return Id;
    }
    menuHandle_t GetHandle() const {
        return Handle;
    }
    VRMenuObject* GetMenuObject() const;

    UIObject* GetParent() const {
        return Parent;
    }

    VRMenuObjectFlags_t const& GetFlags() const;
    void SetFlags(VRMenuObjectFlags_t const& flags);
    void AddFlags(VRMenuObjectFlags_t const& flags);
    void RemoveFlags(VRMenuObjectFlags_t const& flags);

    bool IsHilighted() const;
    void SetHilighted(bool const b);
    bool IsSelected() const;
    virtual void SetSelected(bool const b);

    void SetLocalPose(const OVR::Posef& pose);
    void SetLocalPose(const OVR::Quatf& orientation, const OVR::Vector3f& position);
    OVR::Posef const& GetLocalPose() const;
    OVR::Vector3f const& GetLocalPosition() const;
    void SetLocalPosition(OVR::Vector3f const& pos);
    OVR::Quatf const& GetLocalRotation() const;
    void SetLocalRotation(OVR::Quatf const& rot);
    OVR::Vector3f GetLocalScale() const;
    void SetLocalScale(OVR::Vector3f const& scale);
    void SetLocalScale(float const& scale);

    OVR::Posef GetWorldPose() const;
    OVR::Vector3f GetWorldPosition() const;
    OVR::Quatf GetWorldRotation() const;
    OVR::Vector3f GetWorldScale() const;

    OVR::Vector2f const& GetColorTableOffset() const;
    void SetColorTableOffset(OVR::Vector2f const& ofs);
    OVR::Vector4f const& GetColor() const;
    void SetColor(OVR::Vector4f const& c);
    bool GetVisible() const;
    void SetVisible(const bool visible);

    OVR::Vector2f const& GetDimensions() const {
        return Dimensions;
    } // in local scale
    void SetDimensions(OVR::Vector2f const& dimensions); // in local scale

    UIRectf const& GetBorder() const {
        return Border;
    } // in texel scale
    void SetBorder(UIRectf const& border); // in texel scale
    UIRectf const& GetMargin() const {
        return Margin;
    } // in texel scale
    void SetMargin(UIRectf const& margin); // in texel scale
    UIRectf const& GetPadding() const {
        return Padding;
    } // in texel scale
    void SetPadding(UIRectf const& padding); // in texel scale

    UIRectf GetRect() const; // in texel scale
    UIRectf GetPaddedRect() const; // in texel scale
    UIRectf GetMarginRect() const; // in texel scale

    void AlignTo(
        const RectPosition myPosition,
        const UIObject* other,
        const RectPosition otherPosition,
        const float zOffset = 0.0f);
    void AlignToMargin(
        const RectPosition myPosition,
        const UIObject* other,
        const RectPosition otherPosition,
        const float zOffset = 0.0f);

    void
    SetImage(const int surfaceIndex, const eSurfaceTextureType textureType, char const* imageName);
    void SetImage(
        const int surfaceIndex,
        const eSurfaceTextureType textureType,
        const GLuint image,
        const short width,
        const short height);
    void
    SetImage(const int surfaceIndex, const eSurfaceTextureType textureType, const UITexture& image);
    void SetImage(
        const int surfaceIndex,
        const eSurfaceTextureType textureType,
        const UITexture& image,
        const float dimsX,
        const float dimsY);
    void SetImage(
        const int surfaceIndex,
        const eSurfaceTextureType textureType,
        const UITexture& image,
        const float dimsX,
        const float dimsY,
        const UIRectf& border);
    void SetImage(
        const int surfaceIndex,
        const eSurfaceTextureType textureType,
        const UITexture& image,
        const UIRectf& border);
    void SetImage(const int surfaceIndex, VRMenuSurfaceParms const& parms);
    void SetMultiTextureImage(
        const int surfaceIndex,
        const eSurfaceTextureType textureType,
        const UITexture& image1,
        const UITexture& image2);

    void RegenerateSurfaceGeometry(int const surfaceIndex, const bool freeSurfaceGeometry);
    void RegenerateSurfaceGeometry(const bool freeSurfaceGeometry);

    OVR::Vector2f const& GetSurfaceDims(int const surfaceIndex) const;
    void SetSurfaceDims(
        int const surfaceIndex,
        OVR::Vector2f const& dims); // requires call to RegenerateSurfaceGeometry() to take effect

    UIRectf GetSurfaceBorder(int const surfaceIndex);
    void SetSurfaceBorder(
        int const surfaceIndex,
        UIRectf const& border); // requires call to RegenerateSurfaceGeometry() to take effect

    bool GetSurfaceVisible(int const surfaceIndex) const;
    void SetSurfaceVisible(int const surfaceIndex, const bool visible);

    void SetLocalBoundsExpand(OVR::Vector3f const mins, OVR::Vector3f const& maxs);

    OVR::Bounds3f GetLocalBounds(BitmapFont const& font) const;
    OVR::Bounds3f GetTextLocalBounds(BitmapFont const& font) const;

    void AddComponent(VRMenuComponent* component);
    std::vector<VRMenuComponent*> const& GetComponentList() const;

    void WrapChildrenHorizontal();

   public:
    static float const TEXELS_PER_METER;
    static float const DEFAULT_TEXEL_SCALE;

   protected:
    void AddToMenuWithParms(UIMenu* menu, UIObject* parent, VRMenuObjectParms& parms);

    OvrGuiSys& GuiSys;

    UIMenu* Menu;

    UIObject* Parent;

    VRMenuId_t Id;
    menuHandle_t Handle;
    VRMenuObject* Object;

    OVR::Vector2f Dimensions;
    UIRectf Border;
    UIRectf Margin;
    UIRectf Padding;

    std::vector<UIObject*> Children;

   private:
    // private assignment operator to prevent copying
    UIObject& operator=(UIObject&);
};

} // namespace OVRFW
