/************************************************************************************

Filename    :   SwipeHintComponent.cpp
Content     :
Created     :   Feb 12, 2015
Authors     :   Madhu Kalva, Jim Dose

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "SwipeHintComponent.h"
#include "GazeCursor.h"
#include "GuiSys.h"
#include "VRMenuMgr.h"
#include "VRMenu.h"
#include "PackageFiles.h"
#include "Appl.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {
const char* OvrSwipeHintComponent::TYPE_NAME = "OvrSwipeHintComponent";
bool OvrSwipeHintComponent::ShowSwipeHints = true;

//==============================
//  OvrSwipeHintComponent::OvrSwipeHintComponent()
OvrSwipeHintComponent::OvrSwipeHintComponent(
    const bool isRightSwipe,
    const float totalTime,
    const float timeOffset,
    const float delay)
    : VRMenuComponent(VRMenuEventFlags_t(VRMENU_EVENT_FRAME_UPDATE) | VRMENU_EVENT_OPENING),
      IsRightSwipe(isRightSwipe),
      TotalTime(totalTime),
      TimeOffset(timeOffset),
      Delay(delay),
      StartTime(0),
      ShouldShow(false),
      IgnoreDelay(false),
      TotalAlpha() {
    OVR_UNUSED(IsRightSwipe);
}

//=======================================================
//  OvrSwipeHintComponent::CreateSwipeSuggestionIndicator
menuHandle_t OvrSwipeHintComponent::CreateSwipeSuggestionIndicator(
    OvrGuiSys& guiSys,
    VRMenu* rootMenu,
    const menuHandle_t rootHandle,
    const int menuId,
    const char* img,
    const Posef pose,
    const Vector3f direction) {
    const int NumSwipeTrails = 3;
    int imgWidth, imgHeight;
    OvrVRMenuMgr& menuManager = guiSys.GetVRMenuMgr();
    VRMenuFontParms fontParms(HORIZONTAL_CENTER, VERTICAL_CENTER, false, false, false, 1.0f);

    // Create Menu item for scroll hint root
    VRMenuId_t swipeHintId(menuId);
    std::vector<VRMenuObjectParms const*> parms;
    VRMenuObjectParms parm(
        VRMENU_CONTAINER,
        std::vector<VRMenuComponent*>(),
        VRMenuSurfaceParms(),
        "swipe hint root",
        pose,
        Vector3f(1.0f),
        VRMenuFontParms(),
        swipeHintId,
        VRMenuObjectFlags_t(VRMENUOBJECT_DONT_HIT_ALL),
        VRMenuObjectInitFlags_t(VRMENUOBJECT_INIT_FORCE_POSITION));
    parms.push_back(&parm);
    rootMenu->AddItems(guiSys, parms, rootHandle, false);
    parms.clear();

    menuHandle_t scrollHintHandle = rootMenu->HandleForId(menuManager, swipeHintId);
    assert(scrollHintHandle.IsValid());
    GLuint swipeHintTexture = LoadTextureFromUri(
        guiSys.GetFileSys(),
        (std::string("apk:///") + img).c_str(),
        TextureFlags_t(TEXTUREFLAG_NO_DEFAULT),
        imgWidth,
        imgHeight);
    VRMenuSurfaceParms swipeHintSurfParms(
        "",
        swipeHintTexture,
        imgWidth,
        imgHeight,
        SURFACE_TEXTURE_DIFFUSE,
        0,
        0,
        0,
        SURFACE_TEXTURE_MAX,
        0,
        0,
        0,
        SURFACE_TEXTURE_MAX);

    Posef swipePose = pose;
    for (int i = 0; i < NumSwipeTrails; i++) {
        swipePose.Translation.y =
            (imgHeight * (i + 2)) * 0.5f * direction.y * VRMenuObject::DEFAULT_TEXEL_SCALE;
        swipePose.Translation.z = 0.01f * (float)i;

        std::vector<VRMenuComponent*> hintArrowComps;
        OvrSwipeHintComponent* hintArrowComp =
            new OvrSwipeHintComponent(false, 1.3333f, 0.4f + (float)i * 0.13333f, 5.0f);
        hintArrowComps.push_back(hintArrowComp);
        hintArrowComp->Show(vrapi_GetTimeInSeconds());

        VRMenuObjectParms* swipeIconLeftParms = new VRMenuObjectParms(
            VRMENU_STATIC,
            hintArrowComps,
            swipeHintSurfParms,
            "",
            swipePose,
            Vector3f(1.0f),
            fontParms,
            VRMenuId_t(),
            VRMenuObjectFlags_t(VRMENUOBJECT_FLAG_NO_DEPTH) |
                VRMenuObjectFlags_t(VRMENUOBJECT_DONT_HIT_ALL),
            VRMenuObjectInitFlags_t(VRMENUOBJECT_INIT_FORCE_POSITION));
        parms.push_back(swipeIconLeftParms);
    }

    rootMenu->AddItems(guiSys, parms, scrollHintHandle, false);
    DeletePointerArray(parms);
    parms.clear();

    return scrollHintHandle;
}

//==============================
//  OvrSwipeHintComponent::Reset
void OvrSwipeHintComponent::Reset(VRMenuObject* self) {
    IgnoreDelay = true;
    ShouldShow = false;
    const double now = vrapi_GetTimeInSeconds();
    TotalAlpha.Set(now, TotalAlpha.Value(now), now, 0.0f);
    self->SetColor(Vector4f(1.0f, 1.0f, 1.0f, 0.0f));
}

//==============================
//  OvrSwipeHintComponent::Show
void OvrSwipeHintComponent::Show(const double now) {
    if (!ShouldShow) {
        ShouldShow = true;
        StartTime = now + TimeOffset + (IgnoreDelay ? 0.0f : Delay);
        TotalAlpha.Set(now, TotalAlpha.Value(now), now + 0.5f, 1.0f);
    }
}

//==============================
//  OvrSwipeHintComponent::Hide
void OvrSwipeHintComponent::Hide(const double now) {
    if (ShouldShow) {
        TotalAlpha.Set(now, TotalAlpha.Value(now), now + 0.5f, 0.0f);
        ShouldShow = false;
    }
}

//==============================
//  OvrSwipeHintComponent::OnEvent_Impl
eMsgStatus OvrSwipeHintComponent::OnEvent_Impl(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self,
    VRMenuEvent const& event) {
    switch (event.EventType) {
        case VRMENU_EVENT_OPENING:
            return Opening(guiSys, vrFrame, self, event);
        case VRMENU_EVENT_FRAME_UPDATE:
            return Frame(guiSys, vrFrame, self, event);
        default:
            assert(!"Event flags mismatch!");
            return MSG_STATUS_ALIVE;
    }
}

//==============================
//  OvrSwipeHintComponent::Opening
eMsgStatus OvrSwipeHintComponent::Opening(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self,
    VRMenuEvent const& event) {
    Reset(self);
    return MSG_STATUS_ALIVE;
}

//==============================
//  OvrSwipeHintComponent::Frame
eMsgStatus OvrSwipeHintComponent::Frame(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self,
    VRMenuEvent const& event) {
    if (ShowSwipeHints /* && Carousel->HasSelection() && CanSwipe() */) {
        Show(vrFrame.PredictedDisplayTime);
    } else {
        Hide(vrFrame.PredictedDisplayTime);
    }

    IgnoreDelay = false;

    float alpha = static_cast<float>(TotalAlpha.Value(vrFrame.PredictedDisplayTime));
    if (alpha > 0.0f) {
        double time = vrFrame.PredictedDisplayTime - StartTime;
        if (time < 0.0f) {
            alpha = 0.0f;
        } else {
            double normTime = time / TotalTime;
            alpha *= static_cast<float>(sin(MATH_FLOAT_PI * 2.0 * normTime));
            alpha = std::max<float>(alpha, 0.0f);
        }
    }

    self->SetColor(Vector4f(1.0f, 1.0f, 1.0f, alpha));

    return MSG_STATUS_ALIVE;
}
} // namespace OVRFW
