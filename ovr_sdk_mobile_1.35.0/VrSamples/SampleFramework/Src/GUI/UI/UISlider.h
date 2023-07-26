/************************************************************************************

Filename    :   UISlider.h
Content     :
Created     :	10/01/2015
Authors     :   Jonathan Wright & Warsam Osman

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#pragma once

#include "GUI/VRMenu.h"
#include "UIObject.h"
#include "GUI/SliderComponent.h"

namespace OVRFW {
//==============================================================
// UISlider

class UISlider : public UIObject {
   public:
    UISlider(OvrGuiSys& guiSys);
    ~UISlider();

    void AddToMenu(UIMenu* menu, UIObject* parent = NULL, float startingFrac = 0.5f);
    void SetOnRelease(void (*callback)(UISlider*, void*, float), void* object);

   private:
    OvrSliderComponent* SliderComponent;
    VRMenuId_t VolumeSliderId;
    VRMenuId_t VoumeScrubberId;
    VRMenuId_t VolumeBubbleId;
    VRMenuId_t VolumeFillId;

    void (*OnReleaseFunction)(UISlider* slider, void* object, const float value);
    void* OnReleaseObject;

    friend void
    SliderReleasedCallback(OvrSliderComponent* sliderComponent, void* object, float val);
    void OnRelease(float val);
};

} // namespace OVRFW
