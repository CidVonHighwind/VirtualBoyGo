/************************************************************************************

Filename    :   Slider_Component.h
Content     :   A reusable component implementing a slider bar.
Created     :   Sept 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#pragma once

#include "VRMenuComponent.h"
#include "Fader.h"

namespace OVRFW {

class VRMenu;

//==============================================================
// OvrSliderComponent
class OvrSliderComponent : public VRMenuComponent {
   public:
    static const char* TYPE_NAME;
    static const int TYPE_ID = 0x01010301;

    struct imageInfo_t {
        char const* ImageName;
        int w;
        int h;
    };

    OvrSliderComponent(
        VRMenu& menu,
        float const sliderFrac,
        OVR::Vector3f const& localSlideDelta,
        float const minValue,
        float const maxValue,
        float const sensitivity,
        VRMenuId_t const rootId,
        VRMenuId_t const scrubberId,
        VRMenuId_t const textId,
        VRMenuId_t const bubbleId,
        VRMenuId_t const fillId);

    static void GetVerticalSliderParms(
        VRMenu& menu,
        VRMenuId_t const parentId,
        VRMenuId_t const rootId,
        imageInfo_t const& iconImage,
        imageInfo_t const& sliderBaseImage,
        imageInfo_t const& sliderTrackImage,
        imageInfo_t const& trackFullImage,
        imageInfo_t const& scrubberImage,
        imageInfo_t const& bubbleImage,
        VRMenuId_t const scrubberId,
        VRMenuId_t const bubbleId,
        VRMenuId_t const fillId,
        float const sliderFrac,
        float const minValue,
        float const maxValue,
        float const sensitvityScale,
        std::vector<VRMenuObjectParms const*>& parms);

    virtual char const* GetTypeName() const {
        return TYPE_NAME;
    }
    virtual int GetTypeId() const {
        return TYPE_ID;
    }

    float GetSliderFrac() const {
        return SliderFrac;
    }

    void SetOnRelease(void (*callback)(OvrSliderComponent*, void*, float), void* object);

   private:
    bool TouchDown; // true if
    float SliderFrac; // the current position of the slider represented as a value between 0 and 1.0
    float MinValue; // the minumum value shown on the slider
    float MaxValue; // the maximum value shown on the slider
    float SensitivityScale; // additional multiplyer for sensitivity
    OVR::Vector3f LocalSlideDelta; // the total delta when the slider frac is at 1.0
    VRMenu& Menu; // the menu that will receive updates
    VRMenuId_t RootId; // the id of the slider's root object
    VRMenuId_t ScrubberId; // the caret is the object that slides up and down along the bar
    VRMenuId_t TextId; // the text is an object that always displays the value of the slider,
                       // independent of the bubble
    VRMenuId_t BubbleId; // the bubble is a child of the caret, but offset orthogonal to the bar it
                         // has text that displays the value of the slider bar
    VRMenuId_t FillId; // id for the brightness fill surface
    OVR::Posef CaretBasePose; // the base location of the caret in it's parent's space
    SineFader BubbleFader; // the fader for the bubble's alpha
    double BubbleFadeOutTime; // time when the bubble fader should start fading out
    float
        LastDot; // last dot product deduced from touch... used to apply the movement on a touch up

    // Callback for OnRelease - only called if set
    void (*OnReleaseFunction)(OvrSliderComponent* button, void* object, float val);
    void* OnReleaseObject;

   private:
    OvrSliderComponent& operator=(OvrSliderComponent&);

    virtual eMsgStatus OnEvent_Impl(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event);

    eMsgStatus OnInit(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event);

    eMsgStatus OnFrameUpdate(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event);
    eMsgStatus OnTouchDown(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event);
    eMsgStatus OnTouchUp(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event);
    eMsgStatus OnTouchRelative(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event);

    void GetStringValue(char* valueStr, int maxLen) const;

    void SetCaretPoseFromFrac(OvrVRMenuMgr& menuMgr, VRMenuObject* self, float const frac);
    void UpdateText(OvrVRMenuMgr& menuMgr, VRMenuObject* self, VRMenuId_t const id);
};

} // namespace OVRFW
