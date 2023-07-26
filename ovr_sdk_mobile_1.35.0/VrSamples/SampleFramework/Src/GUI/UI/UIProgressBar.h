/************************************************************************************

Filename    :   UIProgressBar.h
Content     :   Progress bar UI component, with an optional description and cancel button.
Created     :   Mar 11, 2015
Authors     :   Alex Restrepo

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#pragma once

#include "UIObject.h"
#include "UILabel.h"
#include "UIImage.h"
#include "UITexture.h"
#include "UIButton.h"

namespace OVRFW {

class VrAppInterface;

class UIProgressBar : public UIObject {
   public:
    UIProgressBar(OvrGuiSys& guiSys);

    void AddToMenu(
        UIMenu* menu,
        bool showDescriptionLabel,
        bool showCancelButton,
        UIObject* parent = NULL);
    void SetProgress(const float progress);
    void SetDescription(const std::string& description);
    void SetOnCancel(void (*callback)(UIButton*, void*), void* object);
    float GetProgress() const {
        return Progress;
    }
    void SetProgressImageZOffset(float offset);

   private:
    UILabel DescriptionLabel;

    UITexture CancelButtonTexture;
    UIButton CancelButton;

    UITexture ProgressBarTexture;
    UIImage ProgressImage;

    UITexture ProgressBarBackgroundTexture;
    UIImage ProgressBackground;

    float Progress;
};

} // namespace OVRFW
