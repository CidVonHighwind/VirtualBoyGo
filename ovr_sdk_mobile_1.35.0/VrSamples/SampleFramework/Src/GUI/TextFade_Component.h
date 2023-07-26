/************************************************************************************

Filename    :   OvrTextFade_Component.h
Content     :   A reusable component that fades text in and recenters it on gaze over.
Created     :   July 25, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#pragma once

#include "VRMenuComponent.h"
#include "Fader.h"

namespace OVRFW {

//==============================================================
// OvrTextFade_Component
class OvrTextFade_Component : public VRMenuComponent {
   public:
    static double const FADE_DELAY;
    static float const FADE_DURATION;

    OvrTextFade_Component(OVR::Vector3f const& iconBaseOffset, OVR::Vector3f const& iconFadeOffset);

    virtual eMsgStatus OnEvent_Impl(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event);

    eMsgStatus Frame(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event);

    eMsgStatus FocusGained(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event);

    eMsgStatus FocusLost(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event);

    static OVR::Vector3f CalcIconFadeOffset(
        char const* text,
        BitmapFont const& font,
        OVR::Vector3f const& axis,
        float const iconWidth);

   private:
    SineFader TextAlphaFader;
    double StartFadeInTime; // when focus is gained, this is set to the time when the fade in should
                            // begin
    double StartFadeOutTime; // when focus is lost, this is set to the time when the fade out should
                             // begin
    SineFader IconOffsetFader;
    OVR::Vector3f IconBaseOffset; // base offset for text
    OVR::Vector3f IconFadeOffset; // text offset when fully faded
};

} // namespace OVRFW
