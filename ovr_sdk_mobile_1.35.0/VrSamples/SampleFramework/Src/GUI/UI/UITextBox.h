/************************************************************************************

Filename    :   UITextBox.h
Content     :
Created     :	11/05/2015
Authors     :   Eric Duhon

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#pragma once

#include "GUI/VRMenu.h"
#include "UIObject.h"
#include "GUI/VRMenuComponent.h"
#include "UITexture.h"

namespace OVRFW {

class UITextBox;

//==============================================================
// UITextBoxComponent
class UITextBoxComponent : public VRMenuComponent {
   public:
    static const int TYPE_ID = 482932;

    UITextBoxComponent(UITextBox& textBox);
    UITextBoxComponent(const UITextBoxComponent&) = delete;
    UITextBoxComponent& operator=(UITextBoxComponent&) = delete;

    virtual int GetTypeId() const {
        return TYPE_ID;
    }

   private:
    UITextBox& TextBox;

   private:
    virtual eMsgStatus OnEvent_Impl(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event);
};

//==============================================================
// UITextBox

class UITextBox : public UIObject {
    friend class UITextBoxComponent;

   public:
    UITextBox(OvrGuiSys& guiSys);
    ~UITextBox();

    void AddToMenu(UIMenu* menu, UIObject* parent = NULL);

    void AddText(const std::string& text);
    void RemoveLastChar();
    const std::string& GetText() const;
    void SetText(const char* text);
    void SetText(const std::string& text) {
        SetText(text.c_str());
    }

    OVR::Vector4f const& GetTextColor() const;
    void SetTextColor(const OVR::Vector4f& c);

    void SetScale(const OVR::Vector3f& scale);

    void SetCursorBlinkTime(const float blinkTime) {
        CursorBlinkTime = blinkTime;
    }
    void SetMaxCharacters(int maxChars) {
        MaxChars = maxChars;
    }

   private:
    static const char* Cursor;
    void UpdateText();

    UITextBoxComponent* TextBoxComponent{nullptr};

    std::string Text;
    UITexture Background;
    OVR::Vector2f Border{0.005f, 0.002f}; // 5mm border by default on sides, 2mm on top and bottom
    float TextOffset{0};
    bool ShowCursor{false};
    float CursorBlinkTime{0.5f};
    float TimeToFlipCursor{0.0f};
    int MaxChars{100};
};

} // namespace OVRFW
