/************************************************************************************

Filename    :   UIDiscreteSlider.h
Content     :
Created     :	10/04/2015
Authors     :   Warsam Osman

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#pragma once

#include "GUI/VRMenu.h"
#include "UIObject.h"
#include "GUI/VRMenuComponent.h"
#include "UITexture.h"

namespace OVRFW {

class UIDiscreteSliderComponent;

//==============================================================
// UICellComponent
class UICellComponent : public VRMenuComponent {
   public:
    static const char* TYPE_NAME;

    UICellComponent(UIDiscreteSliderComponent& sliderComponent, unsigned int val);
    virtual ~UICellComponent() {}

    virtual const char* GetTypeName() const {
        return TYPE_NAME;
    }

    unsigned int GetValue() const {
        return Value;
    }

   private:
    UIDiscreteSliderComponent& SliderComponent;
    unsigned int Value;

    // make a private assignment operator to prevent warning C4512: assignment operator could not be
    // generated
    UICellComponent& operator=(const UICellComponent&);

    virtual eMsgStatus OnEvent_Impl(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event);

    void (UIDiscreteSliderComponent::*OnClickFunction)(UICellComponent& cell);
    void (UIDiscreteSliderComponent::*OnFocusGainedFunction)(UICellComponent& cell);
    void (UIDiscreteSliderComponent::*OnFocusLostFunction)(UICellComponent& cell);
};

class UIDiscreteSlider;

class UICell : public UIObject {
    friend class UIDiscreteSlider;

   public:
    UICell(OvrGuiSys& guiSys);
    virtual ~UICell() {}

    void AddToDiscreteSlider(UIMenu* menu, UIObject* parent, VRMenuObjectParms& cellParms);
};

//==============================================================
// UIDiscreteSliderComponent
class UIDiscreteSliderComponent : public VRMenuComponent {
    friend class UICellComponent;

   public:
    static const char* TYPE_NAME;

    UIDiscreteSliderComponent(UIDiscreteSlider& discreteSlider, unsigned int startValue);
    virtual ~UIDiscreteSliderComponent() {}

    virtual const char* GetTypeName() const {
        return TYPE_NAME;
    }

    void AddCell(UIObject* cellObject);
    void HighlightCells(unsigned int stopIndex);
    void SetCurrentValue(unsigned int value);
    unsigned int GetCellsCount() const {
        return static_cast<unsigned int>(Cells.size());
    }

   private:
    void OnCellSelect(UICellComponent& cell);
    void OnCellFocusOn(UICellComponent& cell);
    void OnCellFocusOff(UICellComponent& cell);

   private:
    UIDiscreteSlider& DiscreteSlider;

    std::vector<UIObject*> Cells;

    unsigned int CurrentValue;

   private:
    // prevent copying
    UIDiscreteSliderComponent& operator=(UIDiscreteSliderComponent&);
    UIDiscreteSliderComponent(UIDiscreteSliderComponent&);

    virtual eMsgStatus OnEvent_Impl(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event);
};

//==============================================================
// UIDiscreteSlider

class UIDiscreteSlider : public UIObject {
    friend class UIDiscreteSliderComponent;

   public:
    UIDiscreteSlider(OvrGuiSys& guiSys);
    virtual ~UIDiscreteSlider();

    void AddToMenu(UIMenu* menu, UIObject* parent);
    void AddSliderToMenu(UIMenu* menu, UIObject* parent = NULL);
    void AddCells(unsigned int maxValue, unsigned int startValue, float cellSpacing);
    void ScaleCurrentValue(const float scale);
    void SetOnRelease(void (*callback)(UIDiscreteSlider*, void*, float), void* object);
    void SetCellTextures(const UITexture& cellOnTexture, const UITexture& cellOffTexture);
    void SetCellColors(const OVR::Vector4f& cellOnColor, const OVR::Vector4f& cellOffColor);

   private:
    unsigned int MaxValue;
    unsigned int StartValue;
    UIDiscreteSliderComponent* DiscreteSliderComponent;

    UITexture CellOnTexture;
    UITexture CellOffTexture;

    OVR::Vector4f CellOnColor;
    OVR::Vector4f CellOffColor;

    void (*OnReleaseFunction)(UIDiscreteSlider* slider, void* object, const float value);
    void* OnReleaseObject;
    void OnRelease(unsigned int currentValue);
};

} // namespace OVRFW
