/************************************************************************************

Filename    :   UISlider.cpp
Content     :
Created     :	10/01/2015
Authors     :   Jonathan Wright & Warsam Osman

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "UISlider.h"
#include "UIMenu.h"

namespace OVRFW {

UISlider::UISlider(OvrGuiSys& guiSys)
    : UIObject(guiSys),
      SliderComponent(NULL),
      VolumeSliderId(VRMenu::GetRootId().Get() + 9000) // yep... this is terrible! Please fix me!
      ,
      VoumeScrubberId(VRMenu::GetRootId().Get() + 9001),
      VolumeBubbleId(VRMenu::GetRootId().Get() + 9002),
      VolumeFillId(VRMenu::GetRootId().Get() + 9003),
      OnReleaseFunction(NULL),
      OnReleaseObject(NULL) {}

UISlider::~UISlider() {}

void UISlider::AddToMenu(
    UIMenu* menu,
    UIObject* parent /*= NULL */,
    float startingFrac /*= 0.5f*/) {
    Menu = menu;
    Parent = parent;

    Id = VolumeSliderId;

    std::vector<VRMenuObjectParms const*> sliderParms;

    OvrSliderComponent::imageInfo_t iconImage = {"res/raw/nav_brightness_on.png", 80, 80};
    OvrSliderComponent::imageInfo_t sliderBaseImage = {"res/raw/slider_base_%s_small.png", 71, 242};
    OvrSliderComponent::imageInfo_t sliderTrackImage = {"res/raw/slider_track_%s.png", 9, 145};
    OvrSliderComponent::imageInfo_t sliderTrackFullImage = {
        "res/raw/slider_track_full_%s.png", 9, 145};
    OvrSliderComponent::imageInfo_t sliderScrubberImage = {"res/raw/slider_scrubber.png", 20, 20};
    OvrSliderComponent::imageInfo_t sliderBubbleImage = {"res/raw/slider_bubble_%s.png", 59, 52};

    OvrSliderComponent::GetVerticalSliderParms(
        *menu->GetVRMenu(),
        parent->GetId(),
        VolumeSliderId,
        iconImage,
        sliderBaseImage,
        sliderTrackImage,
        sliderTrackFullImage,
        sliderScrubberImage,
        sliderBubbleImage,
        VoumeScrubberId,
        VolumeBubbleId,
        VolumeFillId,
        startingFrac,
        0.0f,
        10.f,
        0.000625f,
        sliderParms);

    menuHandle_t parentHandle =
        (parent == NULL) ? menu->GetVRMenu()->GetRootHandle() : parent->GetHandle();
    Menu->GetVRMenu()->AddItems(GuiSys, sliderParms, parentHandle, false);
    sliderParms.clear();

    if (parent == NULL) {
        Handle = Menu->GetVRMenu()->HandleForId(GuiSys.GetVRMenuMgr(), Id);
    } else {
        std::vector<UIObject*>& children =
            const_cast<std::vector<UIObject*>&>(Parent->GetChildren());
        children.push_back(this);
        Handle = parent->GetMenuObject()->ChildHandleForId(GuiSys.GetVRMenuMgr(), Id);
    }

    VRMenuObject* object = GetMenuObject();
    OVR_ASSERT(object);

    SliderComponent = object->GetComponentByTypeName<OvrSliderComponent>();
    OVR_ASSERT(SliderComponent);
}

void SliderReleasedCallback(OvrSliderComponent* sliderComponent, void* object, float val) {
    ((UISlider*)object)->OnRelease(val);
}

void UISlider::SetOnRelease(void (*callback)(UISlider*, void*, float), void* object) {
    OnReleaseFunction = callback;
    OnReleaseObject = object;

    SliderComponent->SetOnRelease(SliderReleasedCallback, this);
}

void UISlider::OnRelease(float val) {
    if (OnReleaseFunction != NULL) {
        (*OnReleaseFunction)(this, OnReleaseObject, val);
    }
}
} // namespace OVRFW
