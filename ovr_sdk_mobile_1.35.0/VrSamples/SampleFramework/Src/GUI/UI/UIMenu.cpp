/************************************************************************************

Filename    :   UIMenu.cpp
Content     :
Created     :   1/5/2015
Authors     :   Jim Dose

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "UIMenu.h"
#include "GUI/VRMenuMgr.h"
#include "GUI/GuiSys.h"
#include "Appl.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

class UIMenuImpl : public VRMenu {
   public:
    static UIMenuImpl* Create(const char* menuName, UIMenu* menu);

   protected:
    UIMenuImpl(const char* menuName, UIMenu* menu);
    void Frame_Impl(OvrGuiSys& guiSys, ovrApplFrameIn const& vrFrame) override;
    bool OnKeyEvent_Impl(OvrGuiSys& guiSys, int const keyCode, const int action) override;

   private:
    UIMenu* Menu{nullptr};
};

UIMenuImpl::UIMenuImpl(const char* menuName, UIMenu* menu) : VRMenu(menuName), Menu(menu) {}

UIMenuImpl* UIMenuImpl::Create(const char* menuName, UIMenu* menu) {
    return new UIMenuImpl(menuName, menu);
}

void UIMenuImpl::Frame_Impl(OvrGuiSys& guiSys, ovrApplFrameIn const& vrFrame) {
    assert(Menu);
    if (Menu->OnFrameFunction) {
        Menu->OnFrameFunction(Menu, Menu->OnFrameFunctionUsrPtr);
    }
}

bool UIMenuImpl::OnKeyEvent_Impl(OvrGuiSys& guiSys, int const keyCode, const int action) {
    assert(Menu);
    if (Menu->OnKeyEventFunction) {
        return Menu->OnKeyEventFunction(Menu, Menu->OnKeyEventFunctionUsrPtr, keyCode, action);
    }
    return false;
}

UIMenu::UIMenu(OvrGuiSys& guiSys)
    : GuiSys(guiSys),
      MenuName(),
      Menu(NULL),
      MenuOpen(false),
      IdPool(1)

{
    // This is called at library load time, so the system is not initialized
    // properly yet.
}

UIMenu::~UIMenu() {
    Destroy();
}

VRMenuId_t UIMenu::AllocId() {
    VRMenuId_t id = IdPool;
    IdPool = VRMenuId_t(IdPool.Get() + 1);

    return id;
}

void UIMenu::Open() {
    ALOG("Open");
    GuiSys.OpenMenu(MenuName.c_str());
    MenuOpen = true;
}

void UIMenu::Close() {
    ALOG("Close");
    GuiSys.CloseMenu(
        Menu,
        true); /// FIXME: App is not actually used so we pass NULL, but this interface should change
    MenuOpen = false;
}

//=======================================================================================

void UIMenu::Create(const char* menuName) {
    MenuName = menuName;
    Menu = UIMenuImpl::Create(menuName, this);
    Menu->Init(GuiSys, 0.0f, VRMenuFlags_t());
    GuiSys.AddMenu(Menu);
}

void UIMenu::Destroy() {
    if (Menu != nullptr) {
        GuiSys.DestroyMenu(Menu);
        Menu = nullptr;
    }
}

VRMenuFlags_t const& UIMenu::GetFlags() const {
    return Menu->GetFlags();
}

void UIMenu::SetFlags(VRMenuFlags_t const& flags) {
    Menu->SetFlags(flags);
}

Posef const& UIMenu::GetMenuPose() const {
    return Menu->GetMenuPose();
}

void UIMenu::SetMenuPose(Posef const& pose) {
    Menu->SetMenuPose(pose);
}

void UIMenu::SetOnFrameFunction(OnFrameFunctionT onFrameFunction, void* usrPtr) {
    OnFrameFunction = onFrameFunction;
    OnFrameFunctionUsrPtr = usrPtr;
}

void UIMenu::SetOnKeyEventFunction(OnKeyEventFunctionT onKeyEventFunction, void* usrPtr) {
    OnKeyEventFunction = onKeyEventFunction;
    OnKeyEventFunctionUsrPtr = usrPtr;
}

} // namespace OVRFW
