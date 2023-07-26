/************************************************************************************

Filename    :   UIObject.cpp
Content     :
Created     :	1/5/2015
Authors     :   Jim Dose

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "GUI/UI/UIObject.h"
#include "GUI/UI/UIMenu.h"
#include "GUI/UI/UITexture.h"
#include "GUI/VRMenuMgr.h"
#include "GUI/GuiSys.h"
#include "Appl.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

float const UIObject::TEXELS_PER_METER = 500.0f;
float const UIObject::DEFAULT_TEXEL_SCALE = 1.0f / TEXELS_PER_METER;

UIObject::UIObject(OvrGuiSys& guiSys)
    : GuiSys(guiSys),
      Parent(NULL),
      Id(),
      Handle(),
      Dimensions(),
      Border(),
      Margin(),
      Padding(),
      Children()

{}

UIObject::~UIObject() {
    // FIXME: Caught a crash here called from ~VrAppInterface.  Disabling code
    // for now.
#if 0
	if ( Parent != NULL )
	{
		// tell our parent that we're going away
		Parent->RemoveChild( this );
	}

	// we don't know which order the widgets are being freed in, so parents
	// may be deleted before children.  Because of that, we mark any children
	// that haven't been removed yet as not having a parent, so that when they
	// do get deleted, they don't try to remove themselves from a parent that's
	// already been freed.
	for ( size_t i = Children.GetSize() - 1; i >= 0; i-- )
	{
		Children[ i ]->Parent = NULL;
	}
#endif
}

void UIObject::RemoveChild(UIObject* child) {
    int index = -1;
    for (int i = 0; i < static_cast<int>(Children.size()); i++) {
        if (Children[i] == child) {
            index = i;
            break;
        }
    }

    if (index != -1) {
        Children[index]->Parent = NULL;
        Children.erase(Children.cbegin() + index);
    }
}

VRMenuObject* UIObject::GetMenuObject() const {
    return GuiSys.GetVRMenuMgr().ToObject(GetHandle());
}

void UIObject::AddToMenuWithParms(UIMenu* menu, UIObject* parent, VRMenuObjectParms& parms) {
    assert(menu != NULL);
    if (menu == NULL) {
        return;
    }

    Menu = menu;
    Parent = parent;

    Id = parms.Id;

    std::vector<VRMenuObjectParms const*> parmArray;
    parmArray.push_back(&parms);

    menuHandle_t parentHandle =
        (parent == NULL) ? menu->GetVRMenu()->GetRootHandle() : parent->GetHandle();
    Menu->GetVRMenu()->AddItems(GuiSys, parmArray, parentHandle, false);
    parmArray.clear();

    if (parent == NULL) {
        Handle = Menu->GetVRMenu()->HandleForId(GuiSys.GetVRMenuMgr(), Id);
    } else {
        Parent->Children.push_back(this);
        Handle = parent->GetMenuObject()->ChildHandleForId(GuiSys.GetVRMenuMgr(), Id);
    }
}

void UIObject::SetMenuObject(
    UIMenu* menu,
    VRMenuObject* vrMenuObject,
    UIObject* vrMenuObjectParent) {
    assert(menu);
    assert(vrMenuObject);
    assert(Parent == NULL);
    assert(
        vrMenuObjectParent == NULL ||
        (vrMenuObjectParent->GetMenuObject()->GetHandle() == vrMenuObject->GetParentHandle()));
    Parent = vrMenuObjectParent;
    Menu = menu;
    Id = vrMenuObject->GetId();
    Handle = vrMenuObject->GetHandle();
}

VRMenuObjectFlags_t const& UIObject::GetFlags() const {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object == NULL) {
        static VRMenuObjectFlags_t flags;
        return flags;
    }

    return object->GetFlags();
}

void UIObject::SetFlags(VRMenuObjectFlags_t const& flags) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        object->SetFlags(flags);
    }
}

void UIObject::AddFlags(VRMenuObjectFlags_t const& flags) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        object->AddFlags(flags);
    }
}

void UIObject::RemoveFlags(VRMenuObjectFlags_t const& flags) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        object->RemoveFlags(flags);
    }
}

bool UIObject::IsHilighted() const {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        return object->IsHilighted();
    }
    return false;
}

void UIObject::SetHilighted(bool const b) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        object->SetHilighted(b);
    }
}

bool UIObject::IsSelected() const {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        return object->IsSelected();
    }
    return false;
}

void UIObject::SetSelected(bool const b) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        object->SetSelected(b);
    }
}

void UIObject::SetLocalPose(const Posef& pose) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        object->SetLocalPose(pose);
    }
}

void UIObject::SetLocalPose(const Quatf& orientation, const Vector3f& position) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        object->SetLocalPose(Posef(orientation, position));
    }
}

Posef const& UIObject::GetLocalPose() const {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        return object->GetLocalPose();
    } else {
        static Posef pose;
        return pose;
    }
}

void UIObject::SetLocalPosition(Vector3f const& pos) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        object->SetLocalPosition(pos);
    }
}

Vector3f const& UIObject::GetLocalPosition() const {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        return object->GetLocalPosition();
    } else {
        return Vector3f::ZERO;
    }
}

void UIObject::SetLocalRotation(Quatf const& rot) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        object->SetLocalRotation(rot);
    }
}

Quatf const& UIObject::GetLocalRotation() const {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        return object->GetLocalRotation();
    } else {
        static Quatf quat;
        return quat;
    }
}

void UIObject::SetLocalScale(Vector3f const& scale) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        object->SetLocalScale(scale);
    }
}

void UIObject::SetLocalScale(float const& scale) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        object->SetLocalScale(Vector3f(scale));
    }
}

Vector3f UIObject::GetLocalScale() const {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        return object->GetLocalScale();
    } else {
        return Vector3f::ZERO;
    }
}

Posef UIObject::GetWorldPose() const {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object == NULL) {
        return Posef();
    }

    Posef const& localPose = object->GetLocalPose();
    if (Parent == NULL) {
        return localPose;
    }

    Posef parentModelPose = Parent->GetWorldPose();
    Vector3f parentScale = Parent->GetWorldScale();

    Posef curModelPose;
    curModelPose.Translation = parentModelPose.Translation +
        (parentModelPose.Rotation * parentScale.EntrywiseMultiply(localPose.Translation));
    curModelPose.Rotation = localPose.Rotation * parentModelPose.Rotation;

    return curModelPose;
}

Vector3f UIObject::GetWorldPosition() const {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object == NULL) {
        return Vector3f::ZERO;
    }

    Posef const& localPose = object->GetLocalPose();
    if (Parent == NULL) {
        return localPose.Translation;
    }

    Posef parentModelPose = Parent->GetWorldPose();
    Vector3f parentScale = Parent->GetWorldScale();

    return parentModelPose.Translation +
        (parentModelPose.Rotation * parentScale.EntrywiseMultiply(localPose.Translation));
}

Quatf UIObject::GetWorldRotation() const {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object == NULL) {
        return Quatf();
    }

    Quatf const& rotation = object->GetLocalRotation();
    if (Parent == NULL) {
        return rotation;
    }

    return rotation * Parent->GetWorldRotation();
}

Vector3f UIObject::GetWorldScale() const {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object == NULL) {
        return Vector3f::ZERO;
    }

    Vector3f scale = object->GetLocalScale();
    if (Parent == NULL) {
        return scale;
    }

    Vector3f parentScale = Parent->GetWorldScale();
    return parentScale.EntrywiseMultiply(scale);
}

bool UIObject::GetVisible() const {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object == NULL) {
        return false;
    }

    return (object->GetFlags() & VRMENUOBJECT_DONT_RENDER) == 0;
}

void UIObject::SetVisible(const bool visible) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        if (visible) {
            object->RemoveFlags(VRMENUOBJECT_DONT_RENDER);
        } else {
            object->AddFlags(VRMENUOBJECT_DONT_RENDER);
        }
    }
}

void UIObject::SetDimensions(Vector2f const& dimensions) {
    Dimensions = dimensions;

    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object == NULL) {
        return;
    }

    for (int i = 0; i < object->NumSurfaces(); i++) {
        object->SetSurfaceDims(i, Dimensions);
    }
}

void UIObject::SetBorder(UIRectf const& border) {
    Border = border;

    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object == NULL) {
        return;
    }

    Vector4f b(Border.Left, Border.Bottom, Border.Right, Border.Top);
    for (int i = 0; i < object->NumSurfaces(); i++) {
        object->SetSurfaceBorder(i, b);
    }
}

void UIObject::SetMargin(UIRectf const& margin) {
    Margin = margin;
}

void UIObject::SetPadding(UIRectf const& padding) {
    Padding = padding;
}

UIRectf UIObject::GetRect() const {
    UIRectf rect;

    rect.Left = Dimensions.x * -0.5f;
    rect.Bottom = Dimensions.y * -0.5f;
    rect.Right = Dimensions.x * 0.5f;
    rect.Top = Dimensions.y * 0.5f;

    return rect;
}

UIRectf UIObject::GetPaddedRect() const {
    UIRectf rect = GetRect();

    rect.ExpandBy(Padding);

    return rect;
}

UIRectf UIObject::GetMarginRect() const {
    UIRectf rect = GetRect();

    rect.ShrinkBy(Border);
    rect.ShrinkBy(Margin);

    return rect;
}

void UIObject::AlignTo(
    const RectPosition myPosition,
    const UIObject* other,
    const RectPosition otherPosition,
    const float zOffset) {
    const Posef otherWorldPose = other->GetWorldPose();
    const Vector3f otherScale = other->GetWorldScale();

    const Posef parentWorldPose = (Parent == NULL) ? Posef() : Parent->GetWorldPose();
    const Vector3f parentScale = (Parent == NULL) ? Vector3f(1.0f) : Parent->GetWorldScale();

    // get align position in world space
    const UIRectf otherRect = other->GetPaddedRect();
    const Vector3f alignPos = otherRect.GetPosition(otherPosition);
    const Vector3f alignPosWorld =
        otherWorldPose.Rotation.Rotate(alignPos * DEFAULT_TEXEL_SCALE * otherScale) +
        otherWorldPose.Translation;

    // convert align position from world space to parent space
    Vector3f alignPosLocal =
        parentWorldPose.Rotation.Inverted().Rotate(alignPosWorld - parentWorldPose.Translation);
    alignPosLocal.x /= parentScale.x;
    alignPosLocal.y /= parentScale.y;
    alignPosLocal.z /= parentScale.z;

    // calculate other's orientation in relation to our parent and set our orientation to it
    Posef myLocalPose;
    myLocalPose.Rotation = parentWorldPose.Rotation.Inverted() * otherWorldPose.Rotation;

    // calculate the local position of the point being aligned to other and offset the menuobject
    const UIRectf myRect = GetPaddedRect();
    const Vector3f myAlignPos = myRect.GetPosition(myPosition);
    myLocalPose.Translation =
        alignPosLocal - (myAlignPos * DEFAULT_TEXEL_SCALE * GetLocalScale().x);
    myLocalPose.Translation.z += zOffset;
    SetLocalPose(myLocalPose);

#if 0
	LOG( "\n----------------------------" );
	LOG( "otherWorldPose: %s", StringUtils::ToString( otherWorldPose.Position ).ToCStr() );
	LOG( "parentWorldPose: %s", StringUtils::ToString( parentWorldPose.Position ).ToCStr() );
	LOG( "parentScale: %s", StringUtils::ToString( parentScale ).ToCStr() );
	LOG( "otherRect: %f, %f, %f, %f", otherRect.Left, otherRect.Top, otherRect.Right, otherRect.Bottom );
	LOG( "alignPos: %s", StringUtils::ToString( alignPos ).ToCStr() );
	LOG( "alignPosWorld: %s", StringUtils::ToString( alignPosWorld ).ToCStr() );
	LOG( "alignPosLocal: %s", StringUtils::ToString( alignPosLocal ).ToCStr() );
	LOG( "myRect: %f, %f, %f, %f", myRect.Left, myRect.Top, myRect.Right, myRect.Bottom );
	LOG( "myAlignPos: %s", StringUtils::ToString( myAlignPos ).ToCStr() );
	LOG( "myLocalPose.Position: %s", StringUtils::ToString( myLocalPose.Position ).ToCStr() );
	LOG( "FinalWorldPosition: %s", StringUtils::ToString( GetWorldPose().Position ).ToCStr() );
#endif
}

void UIObject::AlignToMargin(
    const RectPosition myPosition,
    const UIObject* other,
    const RectPosition otherPosition,
    const float zOffset) {
    const Posef otherWorldPose = other->GetWorldPose();
    const Vector3f otherScale = other->GetWorldScale();

    const Posef parentWorldPose = (Parent == NULL) ? Posef() : Parent->GetWorldPose();
    const Vector3f parentScale = (Parent == NULL) ? Vector3f(1.0f) : Parent->GetWorldScale();

    // get align position in world space
    const UIRectf otherRect = other->GetMarginRect();
    const Vector3f alignPos = otherRect.GetPosition(otherPosition);
    const Vector3f alignPosWorld =
        otherWorldPose.Rotation.Rotate(alignPos * DEFAULT_TEXEL_SCALE * otherScale) +
        otherWorldPose.Translation;

    // convert align position from world space to parent space
    Vector3f alignPosLocal =
        parentWorldPose.Rotation.Inverted().Rotate(alignPosWorld - parentWorldPose.Translation);
    alignPosLocal.x /= parentScale.x;
    alignPosLocal.y /= parentScale.y;
    alignPosLocal.z /= parentScale.z;

    // calculate other's orientation in relation to our parent and set our orientation to it
    Posef myLocalPose;
    myLocalPose.Rotation = parentWorldPose.Rotation.Inverted() * otherWorldPose.Rotation;

    // calculate the local position of the point being aligned to other and offset the menuobject
    const UIRectf myRect = GetPaddedRect();
    const Vector3f myAlignPos = myRect.GetPosition(myPosition);

    myLocalPose.Translation =
        alignPosLocal - (myAlignPos * DEFAULT_TEXEL_SCALE * GetLocalScale().x);
    myLocalPose.Translation.z += zOffset;
    SetLocalPose(myLocalPose);

#if 0
	LOG( "\n----------------------------" );
	LOG( "otherWorldPose: %s", StringUtils::ToString( otherWorldPose.Position ).ToCStr() );
	LOG( "parentWorldPose: %s", StringUtils::ToString( parentWorldPose.Position ).ToCStr() );
	LOG( "parentScale: %s", StringUtils::ToString( parentScale ).ToCStr() );
	LOG( "otherRect: %f, %f, %f, %f", otherRect.Left, otherRect.Top, otherRect.Right, otherRect.Bottom );
	LOG( "alignPos: %s", StringUtils::ToString( alignPos ).ToCStr() );
	LOG( "alignPosWorld: %s", StringUtils::ToString( alignPosWorld ).ToCStr() );
	LOG( "alignPosLocal: %s", StringUtils::ToString( alignPosLocal ).ToCStr() );
	LOG( "myRect: %f, %f, %f, %f", myRect.Left, myRect.Top, myRect.Right, myRect.Bottom );
	LOG( "myAlignPos: %s", StringUtils::ToString( myAlignPos ).ToCStr() );
	LOG( "myLocalPose.Position: %s", StringUtils::ToString( myLocalPose.Position ).ToCStr() );
	LOG( "FinalWorldPosition: %s", StringUtils::ToString( GetWorldPose().Position ).ToCStr() );
#endif
}

void UIObject::SetImage(
    const int surfaceIndex,
    const eSurfaceTextureType textureType,
    char const* imageName) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object == NULL) {
        return;
    }

    while (object->NumSurfaces() <= surfaceIndex) {
        object->AllocSurface();
    }

    VRMenuSurfaceParms parms(
        "", imageName, textureType, NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX);

    parms.Dims = Dimensions;
    parms.Border = Vector4f(Border.Left, Border.Bottom, Border.Right, Border.Top);

    object->CreateFromSurfaceParms(GuiSys, 0, parms);

    if (Dimensions == Vector2f::ZERO) {
        Dimensions = GetSurfaceDims(0);
    }
}

void UIObject::SetImage(
    const int surfaceIndex,
    const eSurfaceTextureType textureType,
    const GLuint image,
    const short width,
    const short height) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object == NULL) {
        return;
    }

    while (object->NumSurfaces() <= surfaceIndex) {
        object->AllocSurface();
    }

    VRMenuSurfaceParms parms(
        "",
        image,
        width,
        height,
        textureType,
        0,
        0,
        0,
        SURFACE_TEXTURE_MAX,
        0,
        0,
        0,
        SURFACE_TEXTURE_MAX);

    parms.Dims = Dimensions;
    parms.Border = Vector4f(Border.Left, Border.Bottom, Border.Right, Border.Top);

    object->CreateFromSurfaceParms(GuiSys, 0, parms);

    if (Dimensions == Vector2f::ZERO) {
        Dimensions = GetSurfaceDims(0);
    }
}

void UIObject::SetImage(
    const int surfaceIndex,
    const eSurfaceTextureType textureType,
    const UITexture& image) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object == NULL) {
        return;
    }

    while (object->NumSurfaces() <= surfaceIndex) {
        object->AllocSurface();
    }

    VRMenuSurfaceParms parms(
        "",
        image.Texture,
        image.Width,
        image.Height,
        textureType,
        0,
        0,
        0,
        SURFACE_TEXTURE_MAX,
        0,
        0,
        0,
        SURFACE_TEXTURE_MAX);

    parms.Dims = Dimensions;
    parms.Border = Vector4f(Border.Left, Border.Bottom, Border.Right, Border.Top);

    object->CreateFromSurfaceParms(GuiSys, 0, parms);

    if (Dimensions == Vector2f::ZERO) {
        Dimensions = GetSurfaceDims(0);
    }
}

void UIObject::SetMultiTextureImage(
    const int surfaceIndex,
    const eSurfaceTextureType textureType,
    const UITexture& image1,
    const UITexture& image2) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object == NULL) {
        return;
    }

    while (object->NumSurfaces() <= surfaceIndex) {
        object->AllocSurface();
    }

    // single-pass multitexture
    VRMenuSurfaceParms parms(
        "",
        image1.Texture,
        image1.Width,
        image1.Height,
        textureType,
        image2.Texture,
        image2.Width,
        image2.Height,
        textureType,
        0,
        0,
        0,
        SURFACE_TEXTURE_MAX);

    parms.Dims = Dimensions;
    parms.Border = Vector4f(Border.Left, Border.Bottom, Border.Right, Border.Top);

    object->CreateFromSurfaceParms(GuiSys, 0, parms);

    if (Dimensions == Vector2f::ZERO) {
        Dimensions = GetSurfaceDims(0);
    }
}

void UIObject::SetImage(
    const int surfaceIndex,
    const eSurfaceTextureType textureType,
    const UITexture& image,
    const float dimsX,
    const float dimsY) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object == NULL) {
        return;
    }

    while (object->NumSurfaces() <= surfaceIndex) {
        object->AllocSurface();
    }

    VRMenuSurfaceParms parms(
        "",
        image.Texture,
        image.Width,
        image.Height,
        textureType,
        0,
        0,
        0,
        SURFACE_TEXTURE_MAX,
        0,
        0,
        0,
        SURFACE_TEXTURE_MAX);

    parms.Dims.x = dimsX;
    parms.Dims.y = dimsY;

    parms.Border = Vector4f(Border.Left, Border.Bottom, Border.Right, Border.Top);

    object->CreateFromSurfaceParms(GuiSys, 0, parms);

    if (Dimensions == Vector2f::ZERO) {
        Dimensions = GetSurfaceDims(0);
    }
}

void UIObject::SetImage(
    const int surfaceIndex,
    const eSurfaceTextureType textureType,
    const UITexture& image,
    const float dimsX,
    const float dimsY,
    const UIRectf& border) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object == NULL) {
        return;
    }

    while (object->NumSurfaces() <= surfaceIndex) {
        object->AllocSurface();
    }

    VRMenuSurfaceParms parms(
        "",
        image.Texture,
        image.Width,
        image.Height,
        textureType,
        0,
        0,
        0,
        SURFACE_TEXTURE_MAX,
        0,
        0,
        0,
        SURFACE_TEXTURE_MAX);

    parms.Dims.x = dimsX;
    parms.Dims.y = dimsY;
    parms.Border = Vector4f(border.Left, border.Bottom, border.Right, border.Top);

    object->CreateFromSurfaceParms(GuiSys, 0, parms);

    if (Dimensions == Vector2f::ZERO) {
        Dimensions = GetSurfaceDims(0);
    }
}

void UIObject::SetImage(
    const int surfaceIndex,
    const eSurfaceTextureType textureType,
    const UITexture& image,
    const UIRectf& border) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object == NULL) {
        return;
    }

    while (object->NumSurfaces() <= surfaceIndex) {
        object->AllocSurface();
    }

    VRMenuSurfaceParms parms(
        "",
        image.Texture,
        image.Width,
        image.Height,
        textureType,
        0,
        0,
        0,
        SURFACE_TEXTURE_MAX,
        0,
        0,
        0,
        SURFACE_TEXTURE_MAX);

    parms.Dims = Dimensions;
    parms.Border = Vector4f(border.Left, border.Bottom, border.Right, border.Top);

    object->CreateFromSurfaceParms(GuiSys, 0, parms);

    if (Dimensions == Vector2f::ZERO) {
        Dimensions = GetSurfaceDims(0);
    }
}

void UIObject::SetImage(const int surfaceIndex, VRMenuSurfaceParms const& parms) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object == NULL) {
        return;
    }

    while (object->NumSurfaces() <= surfaceIndex) {
        object->AllocSurface();
    }

    VRMenuSurfaceParms localParms = parms;
    if (localParms.Dims == Vector2f::ZERO) {
        localParms.Dims = Dimensions;
    }

    if (localParms.Border == Vector4f::ZERO) {
        localParms.Border = Vector4f(Border.Left, Border.Bottom, Border.Right, Border.Top);
    }

    object->CreateFromSurfaceParms(GuiSys, 0, localParms);

    if (Dimensions == Vector2f::ZERO) {
        Dimensions = GetSurfaceDims(0);
    }
}

void UIObject::SetColorTableOffset(Vector2f const& ofs) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        object->SetColorTableOffset(ofs);
    }
}

Vector2f const& UIObject::GetColorTableOffset() const {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        return object->GetColorTableOffset();
    } else {
        return Vector2f::ZERO;
    }
}

void UIObject::SetColor(Vector4f const& c) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        object->SetColor(c);
    }
}

Vector4f const& UIObject::GetColor() const {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        return object->GetColor();
    } else {
        return Vector4f::ZERO;
    }
}

void UIObject::RegenerateSurfaceGeometry(int const surfaceIndex, const bool freeSurfaceGeometry) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        object->RegenerateSurfaceGeometry(surfaceIndex, freeSurfaceGeometry);
    }
}

void UIObject::RegenerateSurfaceGeometry(const bool freeSurfaceGeometry) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        const int numSurfaces = object->NumSurfaces();
        for (int i = 0; i < numSurfaces; i++) {
            object->RegenerateSurfaceGeometry(i, freeSurfaceGeometry);
        }
    }
}

Vector2f const& UIObject::GetSurfaceDims(int const surfaceIndex) const {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        return object->GetSurfaceDims(surfaceIndex);
    } else {
        return Vector2f::ZERO;
    }
}

void UIObject::SetSurfaceDims(int const surfaceIndex, Vector2f const& dims) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        object->SetSurfaceDims(surfaceIndex, dims);
    }
}

UIRectf UIObject::GetSurfaceBorder(int const surfaceIndex) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        Vector4f rect = object->GetSurfaceBorder(surfaceIndex);
        return UIRectf(rect.x, rect.y, rect.z, rect.w);
    } else {
        return UIRectf();
    }
}

void UIObject::SetSurfaceBorder(int const surfaceIndex, UIRectf const& border) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        Vector4f b(border.Left, border.Bottom, border.Right, border.Top);
        object->SetSurfaceBorder(surfaceIndex, b);
    }
}

bool UIObject::GetSurfaceVisible(int const surfaceIndex) const {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        return object->GetSurfaceVisible(surfaceIndex);
    } else {
        return false;
    }
}

void UIObject::SetSurfaceVisible(int const surfaceIndex, const bool visible) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        object->SetSurfaceVisible(surfaceIndex, visible);
    }
}

void UIObject::SetLocalBoundsExpand(Vector3f const mins, Vector3f const& maxs) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        object->SetLocalBoundsExpand(mins, maxs);
    }
}

Bounds3f UIObject::GetLocalBounds(BitmapFont const& font) const {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        return object->GetLocalBounds(font);
    } else {
        return Bounds3f(Bounds3f::Init);
    }
}

Bounds3f UIObject::GetTextLocalBounds(BitmapFont const& font) const {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        return object->GetTextLocalBounds(font);
    } else {
        return Bounds3f(Bounds3f::Init);
    }
}

void UIObject::AddComponent(VRMenuComponent* component) {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        object->AddComponent(component);
    }
}

std::vector<VRMenuComponent*> const& UIObject::GetComponentList() const {
    VRMenuObject* object = GetMenuObject();
    assert(object);
    if (object != NULL) {
        return object->GetComponentList();
    } else {
        static std::vector<VRMenuComponent*> array;
        return array;
    }
}

void UIObject::WrapChildrenHorizontal() {
    const std::vector<UIObject*>& children = GetChildren();

    // calculate the sum of the widths of all the children
    float totalWidth = 0.0f;
    for (auto* child : children) {
        if (child->GetVisible()) {
            const UIRectf paddedRect = child->GetPaddedRect();
            const float objectWidth =
                paddedRect.GetSize().w * DEFAULT_TEXEL_SCALE * child->GetLocalScale().x;
            totalWidth += objectWidth;
        }
    }

    // reposition children centered horizontally
    float pos = totalWidth * -0.5f;
    for (auto* child : children) {
        if (child->GetVisible()) {
            const UIRectf paddedRect = child->GetPaddedRect();
            const float objectWidth =
                paddedRect.GetSize().w * DEFAULT_TEXEL_SCALE * child->GetLocalScale().x;

            Vector3f localPos = child->GetLocalPosition();
            localPos.x = pos + objectWidth * 0.5f;
            child->SetLocalPosition(localPos);

            pos += objectWidth;
        }
    }

    Vector2f dim = GetDimensions();
    dim.x = totalWidth * TEXELS_PER_METER;
    dim.x += Margin.Left + Margin.Right + Border.Left + Border.Right;
    SetDimensions(dim);
    RegenerateSurfaceGeometry(false);
}

} // namespace OVRFW
