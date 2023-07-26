/************************************************************************************

Filename    :   UINotification.cpp
Content     :   A pop up Notification object
Created     :   Apr 23, 2015
Authors     :   Clint Brewer

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "UINotification.h"
#include "UIMenu.h"
#include "GUI/VRMenuMgr.h"
#include "GUI/GuiSys.h"
#include "Appl.h"
#include "OVR_Math.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

static const float BORDER_SPACING = 28.0f;
static const float ICON_SIZE = 28.0f;

static const float FADE_IN_CONTINUE_TIME =
    0.25f; // how long does it take to fade in if we are continuing from a previous visible message
static const float FADE_IN_FIRST_TIME = 0.75f; // how long does it take if this is the first message

static const float FADE_OUT_CONTINUE_TIME =
    0.5f; // how long does it take to fade out if we are going to display another message
static const float FADE_OUT_LAST_TIME = 2.0f; // how long does it take to fade out the last message

static const float DURATION_CONTINUE_TIME =
    5.0f; // how long does a message stay visible if there are more in the queue
static const float DURATION_LAST_TIME = 10.0f; // how long does the last one stay visible for?

UINotification::UINotification(OvrGuiSys& guiSys)
    : UIObject(guiSys),
      NotificationComponent(NULL),
      BackgroundTintTexture(),
      BackgroundImage(guiSys),
      IconTexture(),
      IconImage(guiSys),
      DescriptionLabel(guiSys),
      VisibleDuration(DURATION_LAST_TIME),
      FadeInDuration(FADE_IN_FIRST_TIME),
      FadeOutDuration(2.0f),
      TimeLeft(0)

{}

UINotification::~UINotification() {
    BackgroundTintTexture.Free();
    IconTexture.Free();
}

void UINotification::AddToMenu(
    UIMenu* menu,
    const char* iconTextureName,
    const char* backgroundTintTextureName,
    UIObject* parent) {
    const Posef pose(Quatf(Vector3f(0.0f, 1.0f, 0.0f), 0.0f), Vector3f(0.0f, 0.0f, 0.0f));

    Vector3f defaultScale(1.0f);

    VRMenuFontParms fontParms(HORIZONTAL_CENTER, VERTICAL_CENTER, false, false, false, 1.0f);

    VRMenuObjectParms parms(
        VRMENU_STATIC,
        std::vector<VRMenuComponent*>(),
        VRMenuSurfaceParms(),
        "",
        pose,
        defaultScale,
        fontParms,
        menu->AllocId(),
        VRMenuObjectFlags_t(),
        VRMenuObjectInitFlags_t(VRMENUOBJECT_INIT_FORCE_POSITION));

    AddToMenuWithParms(menu, parent, parms);

    // textures
    IconTexture.LoadTextureFromUri(
        GuiSys.GetFileSys(), (std::string("apk:///") + iconTextureName).c_str());
    BackgroundTintTexture.LoadTextureFromUri(
        GuiSys.GetFileSys(), (std::string("apk:///") + backgroundTintTextureName).c_str());

    // The Container
    BackgroundImage.AddToMenu(menu, this);
    BackgroundImage.SetImage(
        0,
        SURFACE_TEXTURE_DIFFUSE,
        BackgroundTintTexture,
        300,
        200,
        UIRectf(2.0f, 2.0f, 2.0f, 2.0));
    BackgroundImage.AddFlags(
        VRMenuObjectFlags_t(VRMENUOBJECT_DONT_HIT_ALL) | VRMENUOBJECT_FLAG_NO_FOCUS_GAINED);
    BackgroundImage.SetMargin(
        UIRectf(BORDER_SPACING, BORDER_SPACING, BORDER_SPACING, BORDER_SPACING));

    // The Side Icon
    IconImage.AddToMenu(menu, &BackgroundImage);
    IconImage.SetImage(0, SURFACE_TEXTURE_DIFFUSE, IconTexture, ICON_SIZE, ICON_SIZE);
    IconImage.AddFlags(
        VRMenuObjectFlags_t(VRMENUOBJECT_FLAG_NO_FOCUS_GAINED) | VRMENUOBJECT_DONT_HIT_ALL);
    IconImage.SetPadding(UIRectf(0, 0, BORDER_SPACING * 0.5f + 2, 0));
    Vector3f iconPosition = IconImage.GetLocalPosition();
    iconPosition.z += 0.10f; // offset to prevent z fighting with BackgroundImage,
    IconImage.SetLocalPosition(iconPosition);

    // The Text...
    DescriptionLabel.AddToMenu(menu, &BackgroundImage);
    DescriptionLabel.SetText("");
    DescriptionLabel.SetFontScale(0.5f);
    DescriptionLabel.SetPadding(UIRectf(BORDER_SPACING * 0.5f + 2, 0, 0, 0));
    DescriptionLabel.AlignTo(CENTER, &IconImage, CENTER, 0.00f);

    VRMenuObject* object = GetMenuObject();
    OVR_ASSERT(object);

    NotificationComponent = new UINotificationComponent(*this);
    object->AddComponent(NotificationComponent);

    // make sure we start faded out
    Vector4f color = GetColor();
    color.w = 0.0f;
    SetColor(color);
    SetVisible(false);
}

void UINotification::QueueNotification(
    const std::string& description,
    bool showImmediately /* = false*/) {
    if (showImmediately) {
        SetDescription(description);
    } else {
        if (GetVisible() && TimeLeft > DURATION_CONTINUE_TIME) {
            TimeLeft = DURATION_CONTINUE_TIME;
        }

        NotificationsQueue.push_back(description);
    }
}

void UINotification::SetDescription(const std::string& description) {
    if (DescriptionLabel.GetMenuObject()) {
        DescriptionLabel.SetTextWordWrapped(description.c_str(), GuiSys.GetDefaultFont(), 1.0f);
        DescriptionLabel.CalculateTextDimensions();
        Bounds3f textBounds = DescriptionLabel.GetTextLocalBounds(GuiSys.GetDefaultFont()) *
            VRMenuObject::TEXELS_PER_METER;
        BackgroundImage.SetDimensions(Vector2f(
            textBounds.GetSize().x,
            textBounds.GetSize().y + BORDER_SPACING * 2.0f)); // y is only what matters on this line
        BackgroundImage.WrapChildrenHorizontal();

        bool noMoreInQueue = NotificationsQueue.size() == 0;
        FadeInDuration = GetVisible() ? FADE_IN_CONTINUE_TIME : FADE_IN_FIRST_TIME;
        FadeOutDuration = noMoreInQueue ? FADE_OUT_LAST_TIME : FADE_OUT_CONTINUE_TIME;
        VisibleDuration = noMoreInQueue ? DURATION_LAST_TIME : DURATION_CONTINUE_TIME;
        TimeLeft = VisibleDuration;

        SetVisible(true);
    }
}

void UINotification::Update(float deltaSeconds) {
    TimeLeft -= deltaSeconds;

    if (TimeLeft <= 0) {
        // done, hide or show next one
        TimeLeft = 0;

        if (NotificationsQueue.size() > 0) {
            SetDescription(NotificationsQueue.front());
            NotificationsQueue.pop_front();
        } else {
            SetVisible(false);
        }
    } else if (TimeLeft < FadeOutDuration) {
        // fading out
        float alpha = (TimeLeft / FadeOutDuration);
        Vector4f color = GetColor();
        color.w = clamp<float>(alpha, 0.0f, 1.0f);
        SetColor(color);
    } else if ((VisibleDuration - TimeLeft) < FadeInDuration) {
        // fade in
        float alpha = (VisibleDuration - TimeLeft) / FadeInDuration;
        Vector4f color = GetColor();
        color.w = clamp<float>(alpha, 0.0f, 1.0f);
        SetColor(color);
    } else {
        Vector4f color = GetColor();
        color.w = 1.0f;
        SetColor(color);
    }
}

//===========================================================================================
//  UINotificationComponent::
UINotificationComponent::UINotificationComponent(UINotification& notification)
    : VRMenuComponent(VRMenuEventFlags_t(VRMENU_EVENT_FRAME_UPDATE)), Notification(notification) {}

eMsgStatus UINotificationComponent::OnEvent_Impl(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self,
    VRMenuEvent const& event) {
    switch (event.EventType) {
        case VRMENU_EVENT_FRAME_UPDATE:
            return Frame(guiSys, vrFrame, self, event);
        default:
            OVR_ASSERT(!"Event flags mismatch!");
            return MSG_STATUS_ALIVE;
    }
}

eMsgStatus UINotificationComponent::Frame(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self,
    VRMenuEvent const& event) {
    Notification.Update(vrFrame.DeltaSeconds);
    return MSG_STATUS_ALIVE;
}

} // namespace OVRFW
