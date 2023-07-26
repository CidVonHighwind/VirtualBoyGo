/************************************************************************************

Filename    :   UIDiscreteSlider.cpp
Content     :
Created     :	10/04/2015
Authors     :   Warsam Osman

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/
#include "UIDiscreteSlider.h"
#include "UIMenu.h"
#include "Misc/Log.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

const char* UIDiscreteSliderComponent::TYPE_NAME = "UIDiscreteSliderComponent";

UIDiscreteSlider::UIDiscreteSlider(OvrGuiSys& guiSys)
    : UIObject(guiSys),
      MaxValue(0),
      StartValue(0),
      DiscreteSliderComponent(NULL),
      CellOnTexture(),
      CellOffTexture(),
      CellOnColor(1.0f),
      CellOffColor(1.0f),
      OnReleaseFunction(NULL),
      OnReleaseObject(NULL) {}

UIDiscreteSlider::~UIDiscreteSlider() {}

void UIDiscreteSlider::AddToMenu(UIMenu* menu, UIObject* parent) {
    const Posef pose(Quatf(Vector3f(0.0f, 1.0f, 0.0f), 0.0f), Vector3f(0.0f, 0.0f, 0.0f));

    Vector3f defaultScale(1.0f);
    VRMenuFontParms fontParms(HORIZONTAL_CENTER, VERTICAL_CENTER, false, false, false, 1.0f);

    VRMenuObjectParms parms(
        VRMENU_CONTAINER,
        std::vector<VRMenuComponent*>(),
        VRMenuSurfaceParms(),
        "Container",
        pose,
        defaultScale,
        fontParms,
        menu->AllocId(),
        VRMenuObjectFlags_t(),
        VRMenuObjectInitFlags_t(VRMENUOBJECT_INIT_FORCE_POSITION));

    AddToMenuWithParms(menu, parent, parms);
}

void UIDiscreteSlider::AddSliderToMenu(UIMenu* menu, UIObject* parent /*= NULL */) {
    AddToMenu(menu, parent);
}

void UIDiscreteSlider::AddCells(unsigned int maxValue, unsigned int startValue, float cellSpacing) {
    MaxValue = maxValue;
    StartValue = startValue;

    DiscreteSliderComponent = new UIDiscreteSliderComponent(*this, StartValue);
    OVR_ASSERT(DiscreteSliderComponent);
    AddComponent(DiscreteSliderComponent);

    float cellOffset = 0.0f;
    const float pixelCellSpacing = cellSpacing * VRMenuObject::DEFAULT_TEXEL_SCALE;

    VRMenuFontParms fontParms(HORIZONTAL_CENTER, VERTICAL_CENTER, false, false, false, 1.0f);
    Vector3f defaultScale(1.0f);

    for (unsigned int cellIndex = 0; cellIndex <= MaxValue; ++cellIndex) {
        const Posef pose(Quatf(Vector3f(0.0f, 1.0f, 0.0f), 0.0f), Vector3f(cellOffset, 0.f, 0.0f));

        cellOffset += pixelCellSpacing;

        VRMenuObjectParms cellParms(
            VRMENU_BUTTON,
            std::vector<VRMenuComponent*>(),
            VRMenuSurfaceParms(),
            "",
            pose,
            defaultScale,
            fontParms,
            Menu->AllocId(),
            VRMenuObjectFlags_t(),
            VRMenuObjectInitFlags_t(VRMENUOBJECT_INIT_FORCE_POSITION));

        UICell* cellObject = new UICell(GuiSys);
        cellObject->AddToDiscreteSlider(Menu, this, cellParms);
        cellObject->SetImage(0, SURFACE_TEXTURE_DIFFUSE, CellOffTexture);
        UICellComponent* cellComp = new UICellComponent(*DiscreteSliderComponent, cellIndex);

        VRMenuObject* object = cellObject->GetMenuObject();
        OVR_ASSERT(object);
        object->AddComponent(cellComp);

        DiscreteSliderComponent->AddCell(cellObject);
    }

    DiscreteSliderComponent->HighlightCells(StartValue);
}

void UIDiscreteSlider::ScaleCurrentValue(const float scale) {
    OVR_ASSERT(DiscreteSliderComponent);
    if (scale >= 0.0f && scale <= 1.0f) {
        const unsigned int maxLevel = DiscreteSliderComponent->GetCellsCount();
        DiscreteSliderComponent->SetCurrentValue(static_cast<unsigned int>(maxLevel * scale));
    } else {
        ALOGW("UIDiscreteSlider::SetCurrentValue passed non normal value: %f", scale);
    }
}

void UIDiscreteSlider::SetOnRelease(
    void (*callback)(UIDiscreteSlider*, void*, float),
    void* object) {
    OnReleaseFunction = callback;
    OnReleaseObject = object;
}

void UIDiscreteSlider::SetCellTextures(
    const UITexture& cellOnTexture,
    const UITexture& cellOffTexture) {
    CellOnTexture = cellOnTexture;
    CellOffTexture = cellOffTexture;
}

void UIDiscreteSlider::SetCellColors(const Vector4f& cellOnColor, const Vector4f& cellOffColor) {
    CellOnColor = cellOnColor;
    CellOffColor = cellOffColor;
}

void UIDiscreteSlider::OnRelease(unsigned int currentValue) {
    if (OnReleaseFunction && MaxValue > 0.0f) {
        (*OnReleaseFunction)(
            this, OnReleaseObject, static_cast<float>(currentValue) / static_cast<float>(MaxValue));
    }
}

UICell::UICell(OvrGuiSys& guiSys) : UIObject(guiSys) {}

void UICell::AddToDiscreteSlider(UIMenu* menu, UIObject* parent, VRMenuObjectParms& cellParms) {
    AddToMenuWithParms(menu, parent, cellParms);
}

void UIDiscreteSliderComponent::OnCellSelect(UICellComponent& cell) {
    CurrentValue = cell.GetValue();
    DiscreteSlider.OnRelease(CurrentValue);
}

void UIDiscreteSliderComponent::OnCellFocusOn(UICellComponent& cell) {
    HighlightCells(cell.GetValue());
}

void UIDiscreteSliderComponent::OnCellFocusOff(UICellComponent& cell) {
    HighlightCells(CurrentValue);
}

void UIDiscreteSliderComponent::HighlightCells(unsigned int stopIndex) {
    unsigned int cellIndex = 0;
    for (UIObject* cell : Cells) {
        if (cellIndex <= stopIndex) {
            cell->SetImage(0, SURFACE_TEXTURE_DIFFUSE, DiscreteSlider.CellOnTexture);
            cell->SetColor(DiscreteSlider.CellOnColor);
        } else {
            cell->SetImage(0, SURFACE_TEXTURE_DIFFUSE, DiscreteSlider.CellOffTexture);
            cell->SetColor(DiscreteSlider.CellOffColor);
        }
        cellIndex++;
    }
}

void UIDiscreteSliderComponent::SetCurrentValue(unsigned int value) {
    if (value <= Cells.size()) {
        CurrentValue = value;
        HighlightCells(value);
    } else {
        ALOGW(
            "UIDiscreteSliderComponent::SetCurrentValue - %d outside range %d -> %zu",
            value,
            0,
            Cells.size());
    }
}

UIDiscreteSliderComponent::UIDiscreteSliderComponent(
    UIDiscreteSlider& discreteSlider,
    unsigned int startValue)
    : VRMenuComponent(
          VRMenuEventFlags_t(VRMENU_EVENT_TOUCH_DOWN) | VRMENU_EVENT_TOUCH_UP |
          VRMENU_EVENT_FOCUS_GAINED | VRMENU_EVENT_FOCUS_LOST),
      DiscreteSlider(discreteSlider),
      CurrentValue(startValue) {}

void UIDiscreteSliderComponent::AddCell(UIObject* cellObject) {
    Cells.push_back(cellObject);
}

eMsgStatus UIDiscreteSliderComponent::OnEvent_Impl(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self,
    VRMenuEvent const& event) {
    return MSG_STATUS_ALIVE;
}

const char* UICellComponent::TYPE_NAME = "UICellComponent";

UICellComponent::UICellComponent(UIDiscreteSliderComponent& sliderComponent, unsigned int val)
    : VRMenuComponent(
          VRMenuEventFlags_t(VRMENU_EVENT_TOUCH_UP) | VRMENU_EVENT_FOCUS_GAINED |
          VRMENU_EVENT_FOCUS_LOST),
      SliderComponent(sliderComponent),
      Value(val),
      OnClickFunction(&UIDiscreteSliderComponent::OnCellSelect),
      OnFocusGainedFunction(&UIDiscreteSliderComponent::OnCellFocusOn),
      OnFocusLostFunction(&UIDiscreteSliderComponent::OnCellFocusOff) {}

eMsgStatus UICellComponent::OnEvent_Impl(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self,
    VRMenuEvent const& event) {
    switch (event.EventType) {
        case VRMENU_EVENT_FOCUS_GAINED:
            (SliderComponent.*OnFocusGainedFunction)(*this);
            return MSG_STATUS_ALIVE;
        case VRMENU_EVENT_FOCUS_LOST:
            (SliderComponent.*OnFocusLostFunction)(*this);
            return MSG_STATUS_ALIVE;
        case VRMENU_EVENT_TOUCH_UP:
            (SliderComponent.*OnClickFunction)(*this);
            return MSG_STATUS_CONSUMED;
        default:
            OVR_ASSERT(!"Event flags mismatch!");
            return MSG_STATUS_ALIVE;
    }
}

} // namespace OVRFW
