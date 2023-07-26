/************************************************************************************

Filename    :   UINotification.h
Content     :   A pop up Notification object, intended to be re-usable by many apps.   Lets you
queue up and display info / warning messages Created     :   Apr 23, 2015 Authors     :   Clint
Brewer

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#pragma once

#include "GUI/VRMenu.h"
#include "UIObject.h"
#include "GUI/VRMenuComponent.h"
#include "UITexture.h"
#include "UILabel.h"
#include "UIImage.h"
#include "UIButton.h"

#include <deque>

namespace OVRFW {

class VrAppInterface;
class UINotification;

typedef std::deque<std::string> NotificationsDequeue;

//==============================================================
// UINotificationComponent
class UINotificationComponent : public VRMenuComponent {
   public:
    static const int TYPE_ID = 439493;

    UINotificationComponent(UINotification& notification);

    virtual int GetTypeId() const {
        return TYPE_ID;
    }

   private:
    UINotification& Notification;

   private:
    // private assignment operator to prevent copying
    UINotificationComponent& operator=(UINotificationComponent&);

    virtual eMsgStatus OnEvent_Impl(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event);

    eMsgStatus Frame(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event);
};

//==============================================================
// UINotification

class UINotification : public UIObject {
    friend class UINotificationComponent;

   public:
    UINotification(OvrGuiSys& guiSys);
    ~UINotification();

    void AddToMenu(
        UIMenu* menu,
        const char* iconTextureName,
        const char* backgroundTintTextureName,
        UIObject* parent = NULL);
    void QueueNotification(
        const std::string& description,
        bool showImmediately = false); // will interrupt any currently displaying notice

   private:
    void SetDescription(const std::string& description);
    void Update(float deltaSeconds);

    NotificationsDequeue NotificationsQueue;
    UINotificationComponent* NotificationComponent;

    UITexture BackgroundTintTexture;
    UIImage BackgroundImage;

    UITexture IconTexture;
    UIImage IconImage;

    UILabel DescriptionLabel;

    float VisibleDuration; // how long should this notification be shown
    float FadeInDuration; // how long does it take to fade in
    float FadeOutDuration; // how long does it take to fade out

    float TimeLeft; // how long until this notification is gone
};

} // namespace OVRFW
