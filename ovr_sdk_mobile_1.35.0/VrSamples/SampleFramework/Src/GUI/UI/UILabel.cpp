/************************************************************************************

Filename    :   UILabel.cpp
Content     :
Created     :	1/5/2015
Authors     :   Jim Dose

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "UILabel.h"
#include "UIMenu.h"
#include "GUI/VRMenuMgr.h"
#include "GUI/GuiSys.h"
#include "Appl.h"
#include "Render/BitmapFont.h"
#include "Misc/Log.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

UILabel::UILabel(OvrGuiSys& guiSys)
    : UIObject(guiSys),
      FontParms(HORIZONTAL_CENTER, VERTICAL_CENTER, false, false, false, 1.0f),
      TextOffset(),
      JustificationOffset()

{}

UILabel::~UILabel() {}

void UILabel::AddToMenu(UIMenu* menu, UIObject* parent) {
    const Posef pose(Quatf(Vector3f(0.0f, 1.0f, 0.0f), 0.0f), Vector3f(0.0f, 0.0f, 0.0f));

    VRMenuObjectParms parms(
        VRMENU_STATIC,
        std::vector<VRMenuComponent*>(),
        VRMenuSurfaceParms(),
        "",
        pose,
        Vector3f(1.0f),
        FontParms,
        menu->AllocId(),
        VRMenuObjectFlags_t(),
        VRMenuObjectInitFlags_t(VRMENUOBJECT_INIT_FORCE_POSITION));

    AddToMenuWithParms(menu, parent, parms);
}

void UILabel::SetAlign(const HorizontalJustification horiz, const VerticalJustification vert) {
    FontParms.AlignHoriz = horiz;
    FontParms.AlignVert = vert;
    VRMenuObject* object = GetMenuObject();
    if (object != NULL) {
        object->SetFontParms(FontParms);
        CalculateTextOffset();
    }
}

void UILabel::SetHorizontalAlign(const HorizontalJustification horiz) {
    FontParms.AlignHoriz = horiz;
    VRMenuObject* object = GetMenuObject();
    if (object != NULL) {
        object->SetFontParms(FontParms);
        CalculateTextOffset();
    }
}

HorizontalJustification UILabel::GetHorizontalAlign() const {
    return FontParms.AlignHoriz;
}

void UILabel::SetVerticalAlign(const VerticalJustification vert) {
    FontParms.AlignVert = vert;
    VRMenuObject* object = GetMenuObject();
    if (object != NULL) {
        object->SetFontParms(FontParms);
        CalculateTextOffset();
    }
}

VerticalJustification UILabel::GetVerticalAlign() const {
    return FontParms.AlignVert;
}

void UILabel::SetText(const char* text) {
    VRMenuObject* object = GetMenuObject();
    OVR_ASSERT(object);
    object->SetText(text);
    CalculateTextOffset();
}

void UILabel::SetText(const std::string& text) {
    VRMenuObject* object = GetMenuObject();
    OVR_ASSERT(object);
    object->SetText(text.c_str());
    CalculateTextOffset();
}

void UILabel::CalculateTextOffset() {
    UIRectf rect = GetMarginRect() * DEFAULT_TEXEL_SCALE;

    Vector3f offset = Vector3f::ZERO;
    switch (FontParms.AlignVert) {
        case VERTICAL_BASELINE:
            offset.y = rect.Bottom;
            break;

        case VERTICAL_CENTER: {
            offset.y = 0.0f;
            break;
        }
        case VERTICAL_CENTER_FIXEDHEIGHT: {
            offset.y = 0.0f;
            break;
        }
        case VERTICAL_TOP: {
            offset.y = rect.Top;
            break;
        }
    }

    switch (FontParms.AlignHoriz) {
        case HORIZONTAL_LEFT:
            offset.x = rect.Left;
            break;

        case HORIZONTAL_CENTER: {
            offset.x = 0.0f;
            break;
        }
        case HORIZONTAL_RIGHT: {
            offset.x = rect.Right;
            break;
        }
    }

    JustificationOffset = offset;
    SetTextOffset(TextOffset);
}

void UILabel::CalculateTextDimensions() {
    size_t len;
    int const MAX_LINES = 16;
    float lineWidths[MAX_LINES];
    int numLines = 0;

    float w;
    float h;
    float ascent;
    float descent;
    float fontHeight;

    GuiSys.GetDefaultFont().CalcTextMetrics(
        GetText().c_str(), len, w, h, ascent, descent, fontHeight, lineWidths, MAX_LINES, numLines);

    // NOTE: despite being 3 scalars, text scaling only uses the x component since
    // DrawText3D doesn't take separate x and y scales right now.
    Vector3f const textLocalScale = GetMenuObject()->GetTextLocalScale();
    float const scale = textLocalScale.x * FontParms.Scale;
    // this seems overly complex because font characters are rendered so that their origin
    // is on their baseline and not on one of the corners of the glyph. Because of this
    // we must treat the initial ascent (amount the font goes above the first baseline) and
    // final descent (amount the font goes below the final baseline) independently from the
    // lines in between when centering.
    Bounds3f textBounds(
        Vector3f(0.0f, (h - ascent) * -1.0f, 0.0f) * scale, Vector3f(w, ascent, 0.0f) * scale);

    Vector3f trans = Vector3f::ZERO;
    switch (FontParms.AlignVert) {
        case VERTICAL_BASELINE:
            trans.y = 0.0f;
            break;

        case VERTICAL_CENTER: {
            trans.y = (h * 0.5f) - ascent;
            break;
        }
        case VERTICAL_CENTER_FIXEDHEIGHT: {
            trans.y = (fontHeight * 0.5f);
            break;
        }
        case VERTICAL_TOP: {
            trans.y = h - ascent;
            break;
        }
    }

    switch (FontParms.AlignHoriz) {
        case HORIZONTAL_LEFT:
            trans.x = 0.0f;
            break;

        case HORIZONTAL_CENTER: {
            trans.x = w * -0.5f;
            break;
        }
        case HORIZONTAL_RIGHT: {
            trans.x = w;
            break;
        }
    }

    textBounds.Translate(trans * scale);

    ALOG("textBounds: %f, %f", textBounds.GetSize().x, textBounds.GetSize().y);
    SetDimensions(Vector2f(
        textBounds.GetSize().x * TEXELS_PER_METER, textBounds.GetSize().y * TEXELS_PER_METER));

    CalculateTextOffset();
}

void UILabel::SetTextWordWrapped(
    char const* text,
    class BitmapFont const& font,
    float const widthInMeters) {
    VRMenuObject* object = GetMenuObject();
    OVR_ASSERT(object);
    object->SetTextWordWrapped(text, font, widthInMeters);
}

const std::string& UILabel::GetText() const {
    VRMenuObject* object = GetMenuObject();
    OVR_ASSERT(object);
    return object->GetText();
}

void UILabel::SetFontScale(float const scale) {
    VRMenuObject* object = GetMenuObject();
    OVR_ASSERT(object);

    FontParms.Scale = scale;
    object->SetFontParms(FontParms);
}

float UILabel::GetFontScale() const {
    return FontParms.Scale;
}

void UILabel::SetFontParms(const VRMenuFontParms& parms) {
    FontParms = parms;
    VRMenuObject* object = GetMenuObject();
    if (object != NULL) {
        object->SetFontParms(parms);
    }
}

void UILabel::SetTextOffset(Vector3f const& pos) {
    VRMenuObject* object = GetMenuObject();
    OVR_ASSERT(object);
    TextOffset = pos;
    object->SetTextLocalPosition(TextOffset + JustificationOffset);
}

Vector3f const& UILabel::GetTextOffset() const {
    return TextOffset;
}

void UILabel::SetTextColor(Vector4f const& c) {
    VRMenuObject* object = GetMenuObject();
    OVR_ASSERT(object);
    object->SetTextColor(c);
}

Vector4f const& UILabel::GetTextColor() const {
    VRMenuObject* object = GetMenuObject();
    OVR_ASSERT(object);
    return object->GetTextColor();
}

Bounds3f UILabel::GetTextLocalBounds(BitmapFont const& font) const {
    VRMenuObject* object = GetMenuObject();
    OVR_ASSERT(object);
    return object->GetTextLocalBounds(font);
}

} // namespace OVRFW
