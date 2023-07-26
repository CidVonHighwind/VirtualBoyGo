/************************************************************************************

Filename    :   UILabel.h
Content     :
Created     :	1/5/2015
Authors     :   Jim Dose

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#pragma once

#include "GUI/VRMenu.h"
#include "UIObject.h"

namespace OVRFW {

class VrAppInterface;

class UILabel : public UIObject {
   public:
    UILabel(OvrGuiSys& guiSys);
    ~UILabel();

    void AddToMenu(UIMenu* menu, UIObject* parent = NULL);

    void SetAlign(const HorizontalJustification horiz, const VerticalJustification vert);
    void SetHorizontalAlign(const HorizontalJustification horiz);
    HorizontalJustification GetHorizontalAlign() const;
    void SetVerticalAlign(const VerticalJustification vert);
    VerticalJustification GetVerticalAlign() const;

    void SetText(const char* text);
    void SetText(const std::string& text);
    void
    SetTextWordWrapped(char const* text, class BitmapFont const& font, float const widthInMeters);
    const std::string& GetText() const;

    void CalculateTextDimensions();

    void SetFontScale(float const scale);
    float GetFontScale() const;

    const VRMenuFontParms& GetFontParms() const {
        return FontParms;
    }
    void SetFontParms(const VRMenuFontParms& parms);

    void SetTextOffset(OVR::Vector3f const& pos);
    OVR::Vector3f const& GetTextOffset() const;

    OVR::Vector4f const& GetTextColor() const;
    void SetTextColor(OVR::Vector4f const& c);

    OVR::Bounds3f GetTextLocalBounds(BitmapFont const& font) const;

   private:
    VRMenuFontParms FontParms;
    OVR::Vector3f TextOffset;
    OVR::Vector3f JustificationOffset;

    void CalculateTextOffset();
};

} // namespace OVRFW
