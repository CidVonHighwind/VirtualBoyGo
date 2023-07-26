/************************************************************************************

Filename    :   UITextBox.cpp
Content     :
Created     :	11/05/2015
Authors     :   Eric Duhon

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "UITextBox.h"
#include "UIMenu.h"
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

const char* UITextBox::Cursor = "|";

UITextBox::UITextBox(OvrGuiSys& guiSys) : UIObject(guiSys) {}

UITextBox::~UITextBox() {}

void UITextBox::AddToMenu(UIMenu* menu, UIObject* parent) {
    const Posef pose(Quatf(Vector3f(0.0f, 1.0f, 0.0f), 0.0f), Vector3f(0.0f, 0.0f, 0.0f));

    Vector3f defaultScale(1.0f);
    VRMenuFontParms fontParms(
        HORIZONTAL_LEFT, VERTICAL_CENTER_FIXEDHEIGHT, false, false, false, 1.0f);

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

    Background.LoadTextureFromUri(GuiSys.GetFileSys(), "apk:///res/raw/field_normal.png");
    SetImage(0, eSurfaceTextureType::SURFACE_TEXTURE_DIFFUSE, Background);
    UpdateText();

    VRMenuObject* object = GetMenuObject();
    OVR_ASSERT(object);
    TextBoxComponent = new UITextBoxComponent(*this);
    object->AddComponent(TextBoxComponent);
}

void UITextBox::AddText(const std::string& text) {
    Text += text;
    if (static_cast<int>(Text.length()) > MaxChars) {
        Text.erase(MaxChars, Text.length() - MaxChars);
    }
    UpdateText();
}

void UITextBox::RemoveLastChar() {
    if (!Text.empty()) {
        Text = Text.substr(0, Text.length() - 1);
        UpdateText();
    }
}

void UITextBox::SetText(const char* text) {
    Text = text;
    if (static_cast<int>(Text.length()) > MaxChars) {
        Text.erase(MaxChars, Text.length() - MaxChars);
    }
    UpdateText();
}

const std::string& UITextBox::GetText() const {
    return Text;
}

void UITextBox::SetTextColor(Vector4f const& c) {
    VRMenuObject* object = GetMenuObject();
    OVR_ASSERT(object);
    object->SetTextColor(c);
}

Vector4f const& UITextBox::GetTextColor() const {
    VRMenuObject* object = GetMenuObject();
    OVR_ASSERT(object);
    return object->GetTextColor();
}

void UITextBox::SetScale(Vector3f const& scale) {
    SetLocalScale(scale);

    VRMenuObject* object = GetMenuObject();
    OVR_ASSERT(object);
    VRMenuFontParms fontParms = object->GetFontParms();
    fontParms.Scale = 1.0f / scale.x;
    object->SetFontParms(fontParms);

    TextOffset = GetMarginRect().Left * DEFAULT_TEXEL_SCALE;

    UpdateText();
}

void UITextBox::UpdateText() {
    VRMenuObject* object = GetMenuObject();
    OVR_ASSERT(object);
    VRMenuFontParms fontParms = object->GetFontParms();
    float width = (GetDimensions().x * DEFAULT_TEXEL_SCALE) - (2.0f * Border.x);
    std::string cutText =
        Text + Cursor; // always add space for the caret even if we aren't going to show it
    float extraOffset = GuiSys.GetDefaultFont().GetLastFitChars(cutText, width, fontParms.Scale);
    if (!ShowCursor) {
        cutText.erase(cutText.length() - 1);
    }
    object->SetText(cutText.c_str());

    Vector3f offset = Vector3f::ZERO;
    offset.x = TextOffset + Border.x + extraOffset;
    offset.y = Border.y;
    object->SetTextLocalPosition(offset);
}

/*************************************************************************************/

//==============================
//  UITextBoxComponent::
UITextBoxComponent::UITextBoxComponent(UITextBox& textBox)
    : VRMenuComponent(VRMenuEventFlags_t(VRMENU_EVENT_FRAME_UPDATE)), TextBox(textBox) {}

//==============================
//  UITextBoxComponent::OnEvent_Impl
eMsgStatus UITextBoxComponent::OnEvent_Impl(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self,
    VRMenuEvent const& event) {
    if (event.EventType == VRMENU_EVENT_FRAME_UPDATE) {
        TextBox.TimeToFlipCursor -= vrFrame.DeltaSeconds;
        if (TextBox.TimeToFlipCursor < 0.0f) {
            TextBox.TimeToFlipCursor += TextBox.CursorBlinkTime;
            TextBox.ShowCursor = !TextBox.ShowCursor;
            TextBox.UpdateText();
        }
        return MSG_STATUS_ALIVE;
    }
    OVR_ASSERT(!"Event flags mismatch!");
    return MSG_STATUS_ALIVE;
}

} // namespace OVRFW
