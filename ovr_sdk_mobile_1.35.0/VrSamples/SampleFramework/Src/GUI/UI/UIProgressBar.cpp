/************************************************************************************

Filename    :   UIProgressBar.cpp
Content     :   Progress bar UI component, with an optional description and cancel button.
Created     :   Mar 11, 2015
Authors     :   Alex Restrepo

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "UIProgressBar.h"
#include "UIMenu.h"
#include "UIButton.h"
#include "GUI/VRMenuMgr.h"
#include "Appl.h"
#include "OVR_Math.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

#define DEFAULT_WIDTH 256.0f
#define MIN_WIDTH 228.0f
#define BAR_HEIGHT 6.0f

UIProgressBar::UIProgressBar(OvrGuiSys& guiSys)
    : UIObject(guiSys),
      DescriptionLabel(guiSys),
      CancelButtonTexture(),
      CancelButton(guiSys),
      ProgressBarTexture(),
      ProgressImage(guiSys),
      ProgressBarBackgroundTexture(),
      ProgressBackground(guiSys),
      Progress(0) {}

void UIProgressBar::AddToMenu(
    UIMenu* menu,
    bool showDescriptionLabel,
    bool showCancelButton,
    UIObject* parent) {
    const Posef pose(Quatf(Vector3f(0.0f, 1.0f, 0.0f), 0.0f), Vector3f(0.0f, 0.0f, 0.0f));

    Vector3f defaultScale(1.0f);
    VRMenuFontParms fontParms(HORIZONTAL_LEFT, VERTICAL_CENTER, false, false, false, 1.0f);

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

    // Bar...
    float barWidth = DEFAULT_WIDTH;
    if (parent != NULL) {
        barWidth = parent->GetMarginRect().GetWidth() - 18.0f -
            (showDescriptionLabel ? 0.0f : (showCancelButton ? 40.0f : 0.0f));
        if (barWidth < MIN_WIDTH) {
            barWidth = MIN_WIDTH;
        }
    }

    ProgressBarTexture.LoadTextureFromUri(
        GetGuiSys().GetFileSys(), "apk:///assets/img_progressbar_foreground.png");
    ProgressBarBackgroundTexture.LoadTextureFromUri(
        GetGuiSys().GetFileSys(), "apk:///assets/img_progressbar_background.png");

    ProgressBackground.AddToMenu(menu, this);
    ProgressBackground.SetImage(
        0,
        SURFACE_TEXTURE_DIFFUSE,
        ProgressBarBackgroundTexture,
        barWidth,
        BAR_HEIGHT,
        UIRectf(4.0f, 0.0f, 4.0f, 0.0));
    ProgressBackground.AddFlags(VRMenuObjectFlags_t(VRMENUOBJECT_DONT_HIT_ALL));
    ProgressBackground.AlignTo(TOP_LEFT, this, BOTTOM_LEFT, 0.01f);

    ProgressImage.AddToMenu(menu, &ProgressBackground);
    ProgressImage.SetImage(
        0,
        SURFACE_TEXTURE_DIFFUSE,
        ProgressBarTexture,
        barWidth,
        BAR_HEIGHT,
        UIRectf(4.0f, 0.0f, 4.0f, 0.0));
    ProgressImage.AddFlags(
        VRMenuObjectFlags_t(VRMENUOBJECT_FLAG_NO_FOCUS_GAINED) |
        VRMenuObjectFlags_t(VRMENUOBJECT_DONT_HIT_ALL));
    ProgressImage.AlignTo(
        TOP_LEFT, &ProgressBackground, TOP_LEFT, VRMenuObject::DEFAULT_TEXEL_SCALE);

    // Label...
    if (showDescriptionLabel) {
        DescriptionLabel.AddToMenu(menu, this);
        DescriptionLabel.SetText("");
        DescriptionLabel.SetAlign(HORIZONTAL_LEFT, VERTICAL_CENTER);
        DescriptionLabel.SetPadding(UIRectf(0.0f, 4.0f, 0.0f, 4.0f));
        DescriptionLabel.SetFontScale(0.4f);
        DescriptionLabel.SetTextColor(
            Vector4f(137.0f / 255.0f, 137.0f / 255.0f, 137.0f / 255.0f, 1.0f));

        ProgressBackground.SetPadding(UIRectf(0.0f, 0.0f, 0.0f, 14.0f));
        ProgressBackground.AlignTo(TOP_LEFT, &DescriptionLabel, BOTTOM_LEFT, 0.01f);
    }

    // Cancel Button
    if (showCancelButton) {
        CancelButtonTexture.LoadTextureFromUri(
            GetGuiSys().GetFileSys(), "apk:///assets/img_btn_download_cancel.png");

        CancelButton.AddToMenu(menu, this);
        CancelButton.SetPadding(
            UIRectf(showDescriptionLabel ? barWidth - 28.0f : 0.0f, 0.0f, 0.0f, 0.0f));
        CancelButton.SetDimensions(Vector2f(40.0f, 40.0f));
        CancelButton.SetButtonImages(CancelButtonTexture, CancelButtonTexture, CancelButtonTexture);
        CancelButton.SetButtonColors(
            Vector4f(1.0f), Vector4f(1.0f, 0.5f, 0.0f, 1.0f), Vector4f(1.0f, 0.0f, 0.0f, 1.0f));
        CancelButton.SetLocalPose(Quatf(), Vector3f(0.0f, 0.0f, -0.01f));
        if (showDescriptionLabel) {
            CancelButton.AlignTo(LEFT, this, RIGHT);
        } else {
            CancelButton.AlignTo(LEFT, &ProgressBackground, RIGHT);
        }
    }

    SetProgress(0.0f);
}

void UIProgressBar::SetProgress(const float progress) {
    Progress = clamp<float>(progress, 0.0f, 1.0f);
    float ScrubBarWidth = ProgressBackground.GetDimensions().x;
    float seekwidth = ScrubBarWidth * Progress;

    // The rounded image has a border of 4 px on each side. If the bar is less than 8 px then the
    // borders appear reversed. Clamp to max ( 4.0, width ) if there's any progress to show.
    if (seekwidth < 1.0f) // less than 1 px.
    {
        seekwidth = 0;
    } else {
        seekwidth = std::max(4.0f, seekwidth);
    }

    Vector3f pos = ProgressImage.GetLocalPosition();
    pos.x = ((ScrubBarWidth - seekwidth) * -0.5f) * VRMenuObject::DEFAULT_TEXEL_SCALE;
    ProgressImage.SetLocalPosition(pos);
    ProgressImage.SetSurfaceDims(0, Vector2f(seekwidth, BAR_HEIGHT));
    ProgressImage.RegenerateSurfaceGeometry(0, false);
}

void UIProgressBar::SetDescription(const std::string& description) {
    if (DescriptionLabel.GetMenuObject()) {
        DescriptionLabel.SetText(description);
    }
}

void UIProgressBar::SetOnCancel(void (*callback)(UIButton*, void*), void* object) {
    CancelButton.SetOnClick(callback, object);
}

void UIProgressBar::SetProgressImageZOffset(float offset) {
    ProgressImage.AlignTo(TOP_LEFT, &ProgressBackground, TOP_LEFT, offset);
}

} // namespace OVRFW
