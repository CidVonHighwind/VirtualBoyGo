/************************************************************************************

Filename    :   ProgressBarComponent.h
Content     :   A reusable component implementing a progress bar.
Created     :   Mar 30, 2015
Authors     :   Warsam Osman

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#pragma once

#include "VRMenuComponent.h"
#include "Fader.h"

namespace OVRFW {

class VRMenu;
class App;

//==============================================================
// OvrProgressBarComponent
class OvrProgressBarComponent : public VRMenuComponent {
   public:
    enum eProgressBarState {
        PROGRESSBAR_STATE_NONE,
        PROGRESSBAR_STATE_FADE_IN,
        PROGRESSBAR_STATE_VISIBLE,
        PROGRESSBAR_STATE_FADE_OUT,
        PROGRESSBAR_STATE_HIDDEN,
        PROGRESSBAR_STATES_COUNT
    };

    static const char* TYPE_NAME;

    char const* GetTypeName() const {
        return TYPE_NAME;
    }

    // Get the scrollbar parms and the pointer to the scrollbar component constructed
    static void GetProgressBarParms(
        VRMenu& menu,
        const int width,
        const int height,
        const VRMenuId_t parentId,
        const VRMenuId_t rootId,
        const VRMenuId_t xformId,
        const VRMenuId_t baseId,
        const VRMenuId_t thumbId,
        const VRMenuId_t animId,
        const OVR::Posef& rootLocalPose,
        const OVR::Posef& xformPose,
        const char* baseImage,
        const char* barImage,
        const char* animImage,
        std::vector<const VRMenuObjectParms*>& outParms);

    void SetProgressFrac(OvrVRMenuMgr& menuMgr, VRMenuObject* self, const float frac);
    void SetProgressbarState(VRMenuObject* self, const eProgressBarState state);
    void OneTimeInit(OvrGuiSys& guiSys, VRMenuObject* self, const OVR::Vector4f& color);
    void SetProgressBarBaseWidth(int width) {
        ProgressBarBaseWidth = static_cast<float>(width);
    }
    void SetProgressBarBaseHeight(int height) {
        ProgressBarBaseHeight = static_cast<float>(height);
    }
    void SetProgressBarThumbWidth(int width) {
        ProgressBarThumbWidth = static_cast<float>(width);
    }
    void SetProgressBarThumbHeight(int height) {
        ProgressBarThumbHeight = static_cast<float>(height);
    }

   private:
    OvrProgressBarComponent(
        const VRMenuId_t rootId,
        const VRMenuId_t baseId,
        const VRMenuId_t thumbId,
        const VRMenuId_t animId);

    // private assignment operator to prevent copying
    OvrProgressBarComponent& operator=(OvrProgressBarComponent&);

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

    SineFader Fader; // Used to fade the scroll bar in and out of view
    const float FadeInRate;
    const float FadeOutRate;

    float Frac;

    float ProgressBarBaseWidth;
    float ProgressBarBaseHeight;
    float ProgressBarCurrentbaseLength;
    float ProgressBarThumbWidth;
    float ProgressBarThumbHeight;
    float ProgressBarCurrentThumbLength;

    VRMenuId_t ProgressBarRootId; // Id to the root object which holds the base and thumb
    VRMenuId_t ProgressBarBaseId; // Id to get handle of the base
    VRMenuId_t ProgressBarThumbId; // Id to get the handle of the thumb
    VRMenuId_t ProgressBarAnimId; // Id to get the handle of the progress animation

    eProgressBarState CurrentProgressBarState;
};

} // namespace OVRFW
