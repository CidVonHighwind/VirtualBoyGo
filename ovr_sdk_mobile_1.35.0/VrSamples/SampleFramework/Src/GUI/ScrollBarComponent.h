/************************************************************************************

Filename    :   ScrollBarComponent.h
Content     :   A reusable component implementing a scroll bar.
Created     :   Jan 15, 2014
Authors     :   Warsam Osman

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#pragma once

#include "VRMenuComponent.h"
#include "Fader.h"

namespace OVRFW {

class VRMenu;

//==============================================================
// OvrSliderComponent
class OvrScrollBarComponent : public VRMenuComponent {
   public:
    enum eScrollBarState {
        SCROLL_STATE_NONE,
        SCROLL_STATE_FADE_IN,
        SCROLL_STATE_VISIBLE,
        SCROLL_STATE_FADE_OUT,
        SCROLL_STATE_HIDDEN,
        NUM_SCROLL_STATES
    };

    static const char* TYPE_NAME;

    char const* GetTypeName() const {
        return TYPE_NAME;
    }

    // Get the scrollbar parms and the pointer to the scrollbar component constructed
    static void GetScrollBarParms(
        OvrGuiSys& guiSys,
        VRMenu& menu,
        float scrollBarWidth,
        const VRMenuId_t parentId,
        const VRMenuId_t rootId,
        const VRMenuId_t xformId,
        const VRMenuId_t baseId,
        const VRMenuId_t thumbId,
        const OVR::Posef& rootLocalPose,
        const OVR::Posef& xformPose,
        const int startElementIndex,
        const int numElements,
        const bool verticalBar,
        const OVR::Vector4f& thumbBorder,
        std::vector<const VRMenuObjectParms*>& parms);
    void UpdateScrollBar(OvrVRMenuMgr& menuMgr, VRMenuObject* self, const int numElements);
    void SetScrollFrac(OvrVRMenuMgr& menuMgr, VRMenuObject* self, const float frac);
    void SetScrollState(VRMenuObject* self, const eScrollBarState state);
    void SetBaseColor(OvrVRMenuMgr& menuMgr, VRMenuObject* self, const OVR::Vector4f& color);
    void SetScrollBarBaseWidth(float width) {
        ScrollBarBaseWidth = width;
    }
    void SetScrollBarBaseHeight(float height) {
        ScrollBarBaseHeight = height;
    }
    void SetScrollBarThumbWidth(float width) {
        ScrollBarThumbWidth = width;
    }
    void SetScrollBarThumbHeight(float height) {
        ScrollBarThumbHeight = height;
    }
    void SetIsVertical(bool value) {
        IsVertical = value;
    }

   private:
    OvrScrollBarComponent(
        const VRMenuId_t rootId,
        const VRMenuId_t baseId,
        const VRMenuId_t thumbId,
        const int startElementIndex,
        const int numElements);

    // private assignment operator to prevent copying
    OvrScrollBarComponent& operator=(OvrScrollBarComponent&);

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

    int NumOfItems;
    float ScrollBarBaseWidth;
    float ScrollBarBaseHeight;
    float ScrollBarCurrentbaseLength;
    float ScrollBarThumbWidth;
    float ScrollBarThumbHeight;
    float ScrollBarCurrentThumbLength;

    VRMenuId_t ScrollRootId; // Id to the root object which holds the base and thumb
    VRMenuId_t ScrollBarBaseId; // Id to get handle of the scrollbar base
    VRMenuId_t ScrollBarThumbId; // Id to get the handle of the scrollbar thumb

    eScrollBarState CurrentScrollState;
    bool IsVertical;
};

} // namespace OVRFW
