/************************************************************************************

Filename    :   UIButton.cpp
Content     :
Created     :	1/23/2015
Authors     :   Jim Dose

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "UIButton.h"
#include "UIMenu.h"
#include "GUI/VRMenuMgr.h"
#include "GUI/GuiSys.h"
#include "Appl.h"
#include "Misc/Log.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

UIButton::UIButton(OvrGuiSys& guiSys)
    : UIObject(guiSys),
      ButtonComponent(NULL),
      NormalTexture(),
      HoverTexture(),
      PressedTexture(),
      NormalColor(1.0f),
      HoverColor(1.0f),
      PressedColor(1.0f),
      OnClickFunction(NULL),
      OnClickObject(NULL),
      OnFocusGainedFunction(NULL),
      OnFocusGainedObject(NULL),
      OnFocusLostFunction(NULL),
      OnFocusLostObject(NULL) {}

UIButton::~UIButton() {
    // since components get deleted when the VRMenuObject is deleted, we need to let the
    // component know that it no longer has a valid button.
    if (ButtonComponent != nullptr) {
        ButtonComponent->OwnerDeleted();
        ButtonComponent = nullptr;
    }
}

void UIButton::AddToMenu(UIMenu* menu, UIObject* parent) {
    const Posef pose(Quatf(Vector3f(0.0f, 1.0f, 0.0f), 0.0f), Vector3f(0.0f, 0.0f, 0.0f));

    Vector3f defaultScale(1.0f);
    VRMenuFontParms fontParms(HORIZONTAL_CENTER, VERTICAL_CENTER, false, false, false, 1.0f);

    VRMenuObjectParms parms(
        VRMENU_BUTTON,
        std::vector<VRMenuComponent*>(),
        VRMenuSurfaceParms(),
        "",
        pose,
        defaultScale,
        fontParms,
        menu->AllocId(),
        VRMenuObjectFlags_t(),
        VRMenuObjectInitFlags_t(VRMENUOBJECT_INIT_FORCE_POSITION));

    AddToMenuWithParms(menu, parent, parms);

    VRMenuObject* object = GetMenuObject();
    OVR_ASSERT(object);

    ButtonComponent = new UIButtonComponent(*this);
    object->AddComponent(ButtonComponent);
}

void UIButton::SetButtonImages(
    const UITexture& normal,
    const UITexture& hover,
    const UITexture& pressed,
    const UIRectf& border) {
    NormalTexture = &normal;
    HoverTexture = &hover;
    PressedTexture = &pressed;

    SetBorder(border);

    UpdateButtonState();
}

void UIButton::SetButtonColors(
    const Vector4f& normal,
    const Vector4f& hover,
    const Vector4f& pressed) {
    NormalColor = normal;
    HoverColor = hover;
    PressedColor = pressed;

    UpdateButtonState();
}

void UIButton::SetAsToggleButton(
    const UITexture& pressedHoverTexture,
    const Vector4f& pressedHoverColor) {
    ToggleButton = true;
    PressedHoverColor = pressedHoverColor;
    PressedHoverTexture = &pressedHoverTexture;
}

void UIButton::SetOnClick(void (*callback)(UIButton*, void*), void* object) {
    OnClickFunction = callback;
    OnClickObject = object;
}

void UIButton::SetText(const char* text) {
    VRMenuObject* object = GetMenuObject();
    OVR_ASSERT(object);
    object->SetText(text);
}

void UIButton::SetOnFocusGained(void (*callback)(UIButton*, void*), void* object) {
    OnFocusGainedFunction = callback;
    OnFocusGainedObject = object;
}

void UIButton::SetOnFocusLost(void (*callback)(UIButton*, void*), void* object) {
    OnFocusLostFunction = callback;
    OnFocusLostObject = object;
}

void UIButton::OnClick() {
    if (OnClickFunction != NULL) {
        (*OnClickFunction)(this, OnClickObject);
    }
}

void UIButton::FocusGained() {
    if (OnFocusGainedFunction != NULL) {
        (*OnFocusGainedFunction)(this, OnFocusGainedObject);
    }
}

void UIButton::FocusLost() {
    if (OnFocusLostFunction != NULL) {
        (*OnFocusLostFunction)(this, OnFocusLostObject);
    }
}

//==============================
//  UIButton::UpdateButtonState
void UIButton::UpdateButtonState() {
    const UIRectf border = GetBorder();
    const Vector2f dims = GetDimensions();

    if (ToggleButton && PressedHoverTexture && ButtonComponent->IsPressed() && IsHilighted()) {
        SetImage(0, SURFACE_TEXTURE_DIFFUSE, *PressedHoverTexture, dims.x, dims.y, border);
        SetColor(PressedHoverColor);
    } else if (ButtonComponent->IsPressed()) {
        SetImage(0, SURFACE_TEXTURE_DIFFUSE, *PressedTexture, dims.x, dims.y, border);
        SetColor(PressedColor);
    } else if (IsHilighted()) {
        SetImage(0, SURFACE_TEXTURE_DIFFUSE, *HoverTexture, dims.x, dims.y, border);
        SetColor(HoverColor);
    } else {
        SetImage(0, SURFACE_TEXTURE_DIFFUSE, *NormalTexture, dims.x, dims.y, border);
        SetColor(NormalColor);
    }
}

void UIButton::SetTouchDownSnd(const char* touchDownSnd) {
    if (ButtonComponent) {
        ButtonComponent->SetTouchDownSnd(touchDownSnd);
    }
}

void UIButton::SetTouchUpSnd(const char* touchUpSnd) {
    if (ButtonComponent) {
        ButtonComponent->SetTouchUpSnd(touchUpSnd);
    }
}

/*************************************************************************************/

//==============================
//  UIButtonComponent::
UIButtonComponent::UIButtonComponent(UIButton& button)
    : VRMenuComponent(
          VRMenuEventFlags_t(VRMENU_EVENT_TOUCH_DOWN) | VRMENU_EVENT_TOUCH_UP |
          VRMENU_EVENT_FOCUS_GAINED | VRMENU_EVENT_FOCUS_LOST),
      Button(&button),
      TouchDown(false) {}

UIButtonComponent::~UIButtonComponent() {
    Button = nullptr;
}

//==============================
//  UIButtonComponent::OnEvent_Impl
eMsgStatus UIButtonComponent::OnEvent_Impl(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self,
    VRMenuEvent const& event) {
    if (Button == nullptr) {
        ALOG("UIButtonComponent::OnEvent_Impl - button deleted.");
        return MSG_STATUS_ALIVE;
    }
    switch (event.EventType) {
        case VRMENU_EVENT_FOCUS_GAINED:
            return FocusGained(guiSys, vrFrame, self, event);
        case VRMENU_EVENT_FOCUS_LOST:
            return FocusLost(guiSys, vrFrame, self, event);
        case VRMENU_EVENT_TOUCH_DOWN:
            if (Button->ToggleButton) {
                TouchDown = !TouchDown;
                Button->OnClick();
            } else {
                TouchDown = true;
                if (Button->ActionType == UIButton::eButtonActionType::ClickOnDown) {
                    Button->OnClick();
                }
            }
            Button->UpdateButtonState();
            DownSoundLimiter.PlaySoundEffect(guiSys, TouchDownSnd.c_str(), 0.1);
            return MSG_STATUS_ALIVE;
        case VRMENU_EVENT_TOUCH_UP:
            if (!Button->ToggleButton) {
                TouchDown = false;
            }
            Button->UpdateButtonState();
            if (!Button->ToggleButton &&
                Button->ActionType == UIButton::eButtonActionType::ClickOnUp) {
                Button->OnClick();
            }
            UpSoundLimiter.PlaySoundEffect(guiSys, TouchUpSnd.c_str(), 0.1);
            return MSG_STATUS_ALIVE;
        default:
            OVR_ASSERT(!"Event flags mismatch!");
            return MSG_STATUS_ALIVE;
    }
}

//==============================
//  UIButtonComponent::FocusGained
eMsgStatus UIButtonComponent::FocusGained(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self,
    VRMenuEvent const& event) {
    if (Button == nullptr) {
        return MSG_STATUS_ALIVE;
    }

    // set the hilight flag
    Button->FocusGained();
    self->SetHilighted(true);
    Button->UpdateButtonState();
    GazeOverSoundLimiter.PlaySoundEffect(guiSys, "gaze_on", 0.1);
    return MSG_STATUS_ALIVE;
}

//==============================
//  UIButtonComponent::FocusLost
eMsgStatus UIButtonComponent::FocusLost(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self,
    VRMenuEvent const& event) {
    if (Button == nullptr) {
        return MSG_STATUS_ALIVE;
    }
    // clear the hilight flag
    Button->FocusLost();
    self->SetHilighted(false);
    if (!Button->ToggleButton)
        TouchDown = false;
    Button->UpdateButtonState();
    GazeOverSoundLimiter.PlaySoundEffect(guiSys, "gaze_off", 0.1);
    return MSG_STATUS_ALIVE;
}

void UIButtonComponent::SetTouchDownSnd(const char* touchDownSnd) {
    TouchDownSnd = touchDownSnd;
}

void UIButtonComponent::SetTouchUpSnd(const char* touchUpSnd) {
    TouchUpSnd = touchUpSnd;
}

} // namespace OVRFW
