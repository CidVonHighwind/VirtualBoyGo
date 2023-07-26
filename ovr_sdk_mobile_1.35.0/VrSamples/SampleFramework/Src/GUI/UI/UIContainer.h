/************************************************************************************

Filename    :   UIContainer.h
Content     :
Created     :	1/5/2015
Authors     :   Jim Dose

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#pragma once

#include "GUI/VRMenu.h"
#include "UIObject.h"

namespace OVRFW {

class VrAppInterface;

class UIContainer : public UIObject {
   public:
    UIContainer(OvrGuiSys& guiSys);
    virtual ~UIContainer();

    void AddToMenu(UIMenu* menu, UIObject* parent = NULL);
};

} // namespace OVRFW
