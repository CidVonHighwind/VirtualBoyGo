/************************************************************************************

Filename    :   SwipeHintComponent.h
Content     :
Created     :   Feb 12, 2015
Authors     :   Madhu Kalva, Jim Dose

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#pragma once

#include "VRMenuComponent.h"
#include "OVR_Math.h"

namespace OVRFW {

class VRMenu;

class OvrSwipeHintComponent : public VRMenuComponent {
    class Lerp {
       public:
        void Set(double startDomain_, double startValue_, double endDomain_, double endValue_) {
            startDomain = startDomain_;
            endDomain = endDomain_;
            startValue = startValue_;
            endValue = endValue_;
        }

        double Value(double domain) const {
            const double f =
                clamp<double>((domain - startDomain) / (endDomain - startDomain), 0.0, 1.0);
            return startValue * (1.0 - f) + endValue * f;
        }

        double startDomain;
        double endDomain;
        double startValue;
        double endValue;
    };

   public:
    OvrSwipeHintComponent(
        const bool isRightSwipe,
        const float totalTime,
        const float timeOffset,
        const float delay);

    static menuHandle_t CreateSwipeSuggestionIndicator(
        OvrGuiSys& guiSys,
        VRMenu* rootMenu,
        const menuHandle_t rootHandle,
        const int menuId,
        const char* img,
        const OVR::Posef pose,
        const OVR::Vector3f direction);

    static const char* TYPE_NAME;
    static bool ShowSwipeHints;

    virtual const char* GetTypeName() const {
        return TYPE_NAME;
    }
    void Reset(VRMenuObject* self);

   private:
    bool IsRightSwipe;
    float TotalTime;
    float TimeOffset;
    float Delay;
    double StartTime;
    bool ShouldShow;
    bool IgnoreDelay;
    Lerp TotalAlpha;

   public:
    void Show(const double now);
    void Hide(const double now);
    virtual eMsgStatus OnEvent_Impl(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event);
    eMsgStatus Opening(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event);
    eMsgStatus Frame(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event);
};
} // namespace OVRFW
