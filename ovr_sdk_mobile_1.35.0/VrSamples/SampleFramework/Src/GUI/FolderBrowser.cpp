/************************************************************************************

Filename    :   FolderBrowser.cpp
Content     :   A menu for browsing a hierarchy of folders with items represented by thumbnails.
Created     :   July 25, 2014
Authors     :   Jonathan E. Wright, Warsam Osman, Madhu Kalva

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#include "FolderBrowser.h"

#include <stdio.h>

#include "VRMenuMgr.h"
#include "GuiSys.h"
#include "DefaultComponent.h"

#include "OVR_Math.h"
#include "OVR_Std.h"
#include "stb_image.h"

#include "Misc/Log.h"

#include "AnimComponents.h"
#include "VRMenuObject.h"
#include "ScrollBarComponent.h"
#include "SwipeHintComponent.h"

#include "PackageFiles.h"
#include "OVR_FileSys.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

// Returns true if head equals check plus zero or more characters.
inline bool MatchesHead(const char* head, const char* check) {
    const int l = static_cast<int>(OVR::OVR_strlen(head));
    return 0 == OVR::OVR_strncmp(head, check, l);
}

namespace OVRFW {

const float OvrFolderBrowser::CONTROLER_COOL_DOWN =
    0.2f; // Controller goes to rest very frequently so cool down helps
const float OvrFolderBrowser::SCROLL_DIRECTION_DECIDING_DISTANCE = 10.0f;

char const* OvrFolderBrowser::MENU_NAME = "FolderBrowser";

const Vector3f FWD(0.0f, 0.0f, -1.0f);
const Vector3f LEFT(-1.0f, 0.0f, 0.0f);
const Vector3f RIGHT(1.0f, 0.0f, 0.0f);
const Vector3f UP(0.0f, 1.0f, 0.0f);
const Vector3f DOWN(0.0f, -1.0f, 0.0f);

const float EYE_HEIGHT_OFFSET = 0.0f;
const float MAX_TOUCH_DISTANCE_FOR_TOUCH_SQ = 1800.0f;
const float SCROLL_HITNS_VISIBILITY_TOGGLE_TIME = 5.0f;
const float SCROLL_BAR_LENGTH = 390;
const int HIDE_SCROLLBAR_UNTIL_ITEM_COUNT = 1; // <= 0 makes scroll bar always visible

// Helper class that guarantees unique ids for VRMenuIds
class OvrUniqueId {
   public:
    explicit OvrUniqueId(int startId) : currentId(startId) {}

    int Get(const int increment) {
        const int toReturn = currentId;
        currentId += increment;
        return toReturn;
    }

   private:
    int currentId;
};
OvrUniqueId uniqueId(1000);

VRMenuId_t OvrFolderBrowser::ID_CENTER_ROOT(uniqueId.Get(1));

//==============================
// OvrFolderBrowserRootComponent
// This component is attached to the root parent of the folder browsers and gets to consume input
// first
class OvrFolderBrowserRootComponent : public VRMenuComponent {
   public:
    static const int TYPE_ID = 57514;

    OvrFolderBrowserRootComponent(OvrFolderBrowser& folderBrowser)
        : VRMenuComponent(
              VRMenuEventFlags_t(VRMENU_EVENT_FRAME_UPDATE) | VRMENU_EVENT_TOUCH_RELATIVE |
              VRMENU_EVENT_TOUCH_DOWN | VRMENU_EVENT_TOUCH_UP | VRMENU_EVENT_SWIPE_COMPLETE |
              VRMENU_EVENT_OPENING),
          FolderBrowser(folderBrowser),
          ScrollMgr(VERTICAL_SCROLL),
          TotalTouchDistance(0.0f),
          ValidFoldersCount(0) {
        LastInteractionTimeStamp = vrapi_GetTimeInSeconds() - SCROLL_HITNS_VISIBILITY_TOGGLE_TIME;
        ScrollMgr.SetScrollPadding(0.5f);
    }

    virtual int GetTypeId() const {
        return TYPE_ID;
    }

    int GetCurrentIndex(OvrGuiSys& guiSys, VRMenuObject* self) const {
        VRMenuObject* foldersRootObject = guiSys.GetVRMenuMgr().ToObject(FoldersRootHandle);
        assert(foldersRootObject != NULL);

        // First calculating index of a folder with in valid folders(folder that has at least one
        // panel) array based on its position
        const int validNumFolders = GetFolderBrowserValidFolderCount();
        const float deltaHeight = FolderBrowser.GetPanelHeight();
        const float maxHeight = deltaHeight * validNumFolders;
        const float positionRatio = foldersRootObject->GetLocalPosition().y / maxHeight;
        int idx = static_cast<int>(nearbyintf(positionRatio * validNumFolders));
        idx = clamp<int>(idx, 0, FolderBrowser.GetNumFolders() - 1);

        // Remapping index with in valid folders to index in entire Folder array
        const int numValidFolders = GetFolderBrowserValidFolderCount();
        int counter = 0;
        int remapedIdx = 0;
        for (; remapedIdx < numValidFolders; ++remapedIdx) {
            OvrFolderBrowser::FolderView* folder = FolderBrowser.GetFolderView(remapedIdx);
            if (folder && !folder->Panels.empty()) {
                if (counter == idx) {
                    break;
                }
                ++counter;
            }
        }

        return clamp<int>(remapedIdx, 0, FolderBrowser.GetNumFolders() - 1);
    }

    void SetActiveFolder(int folderIdx) {
        ScrollMgr.SetPosition(static_cast<float>(folderIdx - 1));
    }

    void SetFoldersRootHandle(menuHandle_t handle) {
        FoldersRootHandle = handle;
    }
    void SetScrollDownHintHandle(menuHandle_t handle) {
        ScrollDownHintHandle = handle;
    }
    void SetScrollUpHintHandle(menuHandle_t handle) {
        ScrollUpHintHandle = handle;
    }

   private:
    // private assignment operator to prevent copying
    OvrFolderBrowserRootComponent& operator=(OvrFolderBrowserRootComponent&);

    const static float ACTIVE_DEPTH_OFFSET;

    virtual eMsgStatus OnEvent_Impl(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event) {
        switch (event.EventType) {
            case VRMENU_EVENT_FRAME_UPDATE:
                return Frame(guiSys, vrFrame, self, event);
            case VRMENU_EVENT_TOUCH_DOWN:
                return TouchDown(guiSys, vrFrame, self, event);
            case VRMENU_EVENT_TOUCH_UP:
                return TouchUp(guiSys, vrFrame, self, event);
            case VRMENU_EVENT_SWIPE_COMPLETE: {
                VRMenuEvent upEvent = event;
                upEvent.EventType = VRMENU_EVENT_TOUCH_UP;
                return TouchUp(guiSys, vrFrame, self, upEvent);
            }
            case VRMENU_EVENT_TOUCH_RELATIVE:
                return TouchRelative(guiSys, vrFrame, self, event);
            case VRMENU_EVENT_OPENING:
                return OnOpening(guiSys, vrFrame, self, event);
            default:
                assert(!"Event flags mismatch!"); // the constructor is specifying a flag that's not
                                                  // handled
                return MSG_STATUS_ALIVE;
        }
    }

    eMsgStatus Frame(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event) {
        const int folderCount = FolderBrowser.GetNumFolders();
        ValidFoldersCount = 0;
        // Hides invalid folders(folder that doesn't have at least one panel), and rearranges valid
        // folders positions to avoid gaps between folders
        for (int i = 0; i < folderCount; ++i) {
            const OvrFolderBrowser::FolderView* currentFolder = FolderBrowser.GetFolderView(i);
            if (currentFolder != NULL) {
                VRMenuObject* folderObject = guiSys.GetVRMenuMgr().ToObject(currentFolder->Handle);
                if (folderObject != NULL) {
                    if (currentFolder->Panels.size() > 0) {
                        folderObject->SetLocalPosition(
                            (DOWN * FolderBrowser.GetPanelHeight() *
                             static_cast<float>(ValidFoldersCount)));
                        ++ValidFoldersCount;
                    }
                }
            }
        }

        bool restrictedScrolling = ValidFoldersCount > 1;
        eScrollDirectionLockType touchDirLock = FolderBrowser.GetTouchDirectionLock();
        eScrollDirectionLockType controllerDirLock = FolderBrowser.GetControllerDirectionLock();

        ScrollMgr.SetMaxPosition(static_cast<float>(ValidFoldersCount - 1));
        ScrollMgr.SetRestrictedScrollingData(restrictedScrolling, touchDirLock, controllerDirLock);

        unsigned int controllerInput = 0;
        if (ValidFoldersCount > 1) // Need at least one folder in order to enable vertical scrolling
        {
            controllerInput = vrFrame.AllButtons;
            if (controllerInput &
                (ovrButton_Up | ovrButton_Down | ovrButton_Left | ovrButton_Right)) {
                LastInteractionTimeStamp = vrapi_GetTimeInSeconds();
            }

            if (restrictedScrolling) {
                if (touchDirLock == HORIZONTAL_LOCK) {
                    if (ScrollMgr.IsTouchIsDown()) {
                        // Restricted scrolling enabled, but locked horizontal scrolling so ignore
                        // vertical scrolling
                        ScrollMgr.TouchUp();
                    }
                }

                if (controllerDirLock != VERTICAL_LOCK) {
                    // Restricted scrolling enabled, but not locked to vertical scrolling so lose
                    // the controller input
                    controllerInput = 0;
                }
            }
        }
        ScrollMgr.Frame(vrFrame.DeltaSeconds, controllerInput);

        VRMenuObject* foldersRootObject = guiSys.GetVRMenuMgr().ToObject(FoldersRootHandle);
        assert(foldersRootObject != NULL);
        const Vector3f& rootPosition = foldersRootObject->GetLocalPosition();
        foldersRootObject->SetLocalPosition(Vector3f(
            rootPosition.x,
            FolderBrowser.GetPanelHeight() * ScrollMgr.GetPosition(),
            rootPosition.z));

        const float alphaSpace = FolderBrowser.GetPanelHeight() * 2.0f;
        const float rootOffset = rootPosition.y - EYE_HEIGHT_OFFSET;

        // Fade + hide each category/folder based on distance from origin
        for (int folderIndex = 0; folderIndex < FolderBrowser.GetNumFolders(); ++folderIndex) {
            OvrFolderBrowser::FolderView* folder = FolderBrowser.GetFolderView(folderIndex);
            VRMenuObject* child = guiSys.GetVRMenuMgr().ToObject(
                foldersRootObject->GetChildHandleForIndex(folderIndex));
            assert(child != NULL);

            const Vector3f& position = child->GetLocalPosition();
            Vector4f color = child->GetColor();
            color.w = 0.0f;
            const bool showFolder = FolderBrowser.HasNoMedia() || (folder->Panels.size() > 0);

            VRMenuObjectFlags_t flags = child->GetFlags();
            const float absolutePosition = fabsf(rootOffset - fabsf(position.y));
            if (absolutePosition < alphaSpace && showFolder) {
                // Fade in / out
                float ratio = absolutePosition / alphaSpace;
                float ratioSq = ratio * ratio;
                float finalAlpha = 1.0f - ratioSq;
                color.w = clamp<float>(finalAlpha, 0.0f, 1.0f);

                // If we're showing the category for the first time, load the thumbs
                // Only queue up if velocity is below threshold to avoid queuing categories flying
                // past view
                // const float scrollVelocity = ScrollMgr.GetVelocity();
                if (!folder->Visible /*&& ( fabsf( scrollVelocity ) < 0.09f )*/) {
                    ALOG("Revealing %s - loading thumbs", folder->CategoryTag.c_str());
                    folder->Visible = true;
                    // This loads entire category - instead we load specific thumbs in
                    // SwipeComponent
                }

                flags &=
                    ~(VRMenuObjectFlags_t(VRMENUOBJECT_DONT_RENDER) | VRMENUOBJECT_DONT_HIT_ALL);

                // Lerp the folder towards or away from viewer
                Vector3f activePosition = position;
                const float targetZ = ACTIVE_DEPTH_OFFSET * finalAlpha;
                activePosition.z = targetZ;
                child->SetLocalPosition(activePosition);
            } else {
                // If we're just about to hide the folderview, unload the thumbnails
                if (folder->Visible) {
                    ALOG("Hiding %s - unloading thumbs", folder->CategoryTag.c_str());
                    folder->Visible = false;
                    folder->UnloadThumbnails(
                        guiSys,
                        FolderBrowser.GetDefaultThumbnailTextureId(),
                        FolderBrowser.GetThumbWidth(),
                        FolderBrowser.GetThumbHeight());
                }

                flags |= VRMenuObjectFlags_t(VRMENUOBJECT_DONT_RENDER) | VRMENUOBJECT_DONT_HIT_ALL;
            }

            child->SetColor(color);
            child->SetFlags(flags);
        }

        // Updating Scroll Suggestions
        bool showScrollUpIndicator = false;
        bool showBottomIndicator = false;
        Vector4f finalCol(1.0f, 1.0f, 1.0f, 1.0f);
        if (LastInteractionTimeStamp >
            0.0f) // is user interaction currently going on? ( during interaction
                  // LastInteractionTimeStamp value will be negative )
        {
            float timeDiff =
                static_cast<float>(vrapi_GetTimeInSeconds() - LastInteractionTimeStamp);
            if (timeDiff >
                SCROLL_HITNS_VISIBILITY_TOGGLE_TIME) // is user not interacting for a while?
            {
                // Show Indicators
                showScrollUpIndicator = (ScrollMgr.GetPosition() > 0.8f);
                showBottomIndicator = (ScrollMgr.GetPosition() < ((ValidFoldersCount - 1) - 0.8f));

                finalCol.w = clamp<float>(timeDiff, 5.0f, 6.0f) - 5.0f;
            }
        }

        VRMenuObject* scrollUpHintObject = guiSys.GetVRMenuMgr().ToObject(ScrollUpHintHandle);
        if (scrollUpHintObject != NULL) {
            scrollUpHintObject->SetVisible(showScrollUpIndicator);
            scrollUpHintObject->SetColor(finalCol);
        }

        VRMenuObject* scrollDownHintObject = guiSys.GetVRMenuMgr().ToObject(ScrollDownHintHandle);
        if (scrollDownHintObject != NULL) {
            scrollDownHintObject->SetVisible(showBottomIndicator);
            scrollDownHintObject->SetColor(finalCol);
        }

        return MSG_STATUS_ALIVE;
    }

    eMsgStatus TouchDown(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event) {
        if (FolderBrowser.HasNoMedia()) {
            return MSG_STATUS_CONSUMED;
        }

        FolderBrowser.TouchDown();
        if (ValidFoldersCount > 1) {
            ScrollMgr.TouchDown();
        }

        TotalTouchDistance = 0.0f;
        StartTouchPosition.x = event.FloatValue.x;
        StartTouchPosition.y = event.FloatValue.y;

        LastInteractionTimeStamp = -1.0f;

        return MSG_STATUS_ALIVE; // don't consume -- we're just listening
    }

    eMsgStatus TouchUp(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event) {
        if (FolderBrowser.HasNoMedia()) {
            return MSG_STATUS_CONSUMED;
        }

        FolderBrowser.TouchUp();
        if (ValidFoldersCount > 1) {
            ScrollMgr.TouchUp();
        }

        bool allowPanelTouchUp = false;
        if (TotalTouchDistance < MAX_TOUCH_DISTANCE_FOR_TOUCH_SQ) {
            allowPanelTouchUp = true;
        }

        FolderBrowser.SetAllowPanelTouchUp(allowPanelTouchUp);
        LastInteractionTimeStamp = vrapi_GetTimeInSeconds();

        return MSG_STATUS_ALIVE;
    }

    eMsgStatus TouchRelative(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event) {
        if (FolderBrowser.HasNoMedia()) {
            return MSG_STATUS_CONSUMED;
        }

        FolderBrowser.TouchRelative(event.FloatValue);
        Vector2f currentTouchPosition(event.FloatValue.x, event.FloatValue.y);
        TotalTouchDistance += (currentTouchPosition - StartTouchPosition).LengthSq();
        if (ValidFoldersCount > 1) {
            ScrollMgr.TouchRelative(event.FloatValue, vrFrame.RealTimeInSeconds);
        }

        return MSG_STATUS_ALIVE;
    }

    eMsgStatus OnOpening(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event) {
        return MSG_STATUS_ALIVE;
    }

    // Return valid folder count, folder that has atleast one panel is considered as valid folder.
    int GetFolderBrowserValidFolderCount() const {
        int validFoldersCount = 0;
        const int folderCount = FolderBrowser.GetNumFolders();
        for (int i = 0; i < folderCount; ++i) {
            const OvrFolderBrowser::FolderView* currentFolder = FolderBrowser.GetFolderView(i);
            if (currentFolder && currentFolder->Panels.size() > 0) {
                ++validFoldersCount;
            }
        }
        return validFoldersCount;
    }

    OvrFolderBrowser& FolderBrowser;
    OvrScrollManager ScrollMgr;
    Vector2f StartTouchPosition;
    float TotalTouchDistance;
    int ValidFoldersCount;
    menuHandle_t FoldersRootHandle;
    menuHandle_t ScrollDownHintHandle;
    menuHandle_t ScrollUpHintHandle;
    double LastInteractionTimeStamp;
};

const float OvrFolderBrowserRootComponent::ACTIVE_DEPTH_OFFSET = 3.4f;

//==============================================================
// OvrFolderRootComponent
// Component attached to the root object of each folder
class OvrFolderRootComponent : public VRMenuComponent {
   public:
    OvrFolderRootComponent(OvrFolderBrowser& folderBrowser, OvrFolderBrowser::FolderView* folder)
        : VRMenuComponent(VRMenuEventFlags_t(VRMENU_EVENT_FRAME_UPDATE)),
          FolderBrowser(folderBrowser),
          FolderPtr(folder) {
        assert(FolderPtr);
    }

   private:
    // private assignment operator to prevent copying
    OvrFolderRootComponent& operator=(OvrFolderRootComponent&);

    virtual eMsgStatus OnEvent_Impl(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event) {
        switch (event.EventType) {
            case VRMENU_EVENT_FRAME_UPDATE:
                return Frame(guiSys, vrFrame, self, event);
            default:
                assert(!"Event flags mismatch!"); // the constructor is specifying a flag that's not
                                                  // handled
                return MSG_STATUS_ALIVE;
        }
    }

    eMsgStatus Frame(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event) {
        VRMenuObjectFlags_t flags = self->GetFlags();
        assert(FolderPtr);
        if (FolderBrowser.GetFolderView(FolderBrowser.GetActiveFolderIndex(guiSys)) != FolderPtr) {
            flags |= VRMenuObjectFlags_t(VRMENUOBJECT_DONT_HIT_ALL);
        } else {
            flags &= ~VRMenuObjectFlags_t(VRMENUOBJECT_DONT_HIT_ALL);
        }
        self->SetFlags(flags);

        return MSG_STATUS_ALIVE;
    }

   private:
    OvrFolderBrowser& FolderBrowser;
    OvrFolderBrowser::FolderView* FolderPtr;
};

//==============================================================
// OvrFolderSwipeComponent
// Component that holds panel sub-objects and manages swipe left/right
class OvrFolderSwipeComponent : public VRMenuComponent {
   public:
    static const int TYPE_ID = 58524;

    OvrFolderSwipeComponent(OvrFolderBrowser& folderBrowser, OvrFolderBrowser::FolderView* folder)
        : VRMenuComponent(
              VRMenuEventFlags_t(VRMENU_EVENT_FRAME_UPDATE) | VRMENU_EVENT_TOUCH_RELATIVE |
              VRMENU_EVENT_TOUCH_DOWN | VRMENU_EVENT_TOUCH_UP | VRMENU_EVENT_SWIPE_COMPLETE),
          FolderBrowser(folderBrowser),
          ScrollMgr(HORIZONTAL_SCROLL),
          FolderPtr(folder),
          TouchDown(false) {
        assert(FolderBrowser.GetCircumferencePanelSlots() > 0);
        ScrollMgr.SetScrollPadding(0.5f);
    }

    virtual int GetTypeId() const {
        return TYPE_ID;
    }

    bool Rotate(const OvrFolderBrowser::CategoryDirection direction) {
        static float const MAX_SPEED = 5.5f;
        switch (direction) {
            case OvrFolderBrowser::MOVE_PANELS_LEFT:
                ScrollMgr.SetVelocity(MAX_SPEED);
                return true;
            case OvrFolderBrowser::MOVE_PANELS_RIGHT:
                ScrollMgr.SetVelocity(-MAX_SPEED);
                return true;
            default:
                return false;
        }
    }

    void SetRotationByRatio(const float ratio) {
        assert(ratio >= 0.0f && ratio <= 1.0f);
        ScrollMgr.SetPosition(FolderPtr->MaxRotation * ratio);
        ScrollMgr.SetVelocity(0.0f);
    }

    void SetRotationByIndex(const int panelIndex) {
        assert(panelIndex >= 0 && panelIndex < static_cast<int>((*FolderPtr).Panels.size()));
        ScrollMgr.SetPosition(static_cast<float>(panelIndex));
    }

    void SetAccumulatedRotation(const float rot) {
        ScrollMgr.SetPosition(rot);
        ScrollMgr.SetVelocity(0.0f);
    }

    float GetAccumulatedRotation() const {
        return ScrollMgr.GetPosition();
    }

    int CurrentPanelIndex() const {
        return static_cast<int>(nearbyintf(ScrollMgr.GetPosition()));
    }

    int GetNextPanelIndex(const int step) const {
        const OvrFolderBrowser::FolderView& folder = *FolderPtr;
        const int numPanels = static_cast<int>(folder.Panels.size());

        int nextPanelIndex = CurrentPanelIndex() + step;
        if (nextPanelIndex >= numPanels) {
            nextPanelIndex = 0;
        } else if (nextPanelIndex < 0) {
            nextPanelIndex = numPanels - 1;
        }

        return nextPanelIndex;
    }

   private:
    // private assignment operator to prevent copying
    OvrFolderSwipeComponent& operator=(OvrFolderSwipeComponent&);

    virtual eMsgStatus Frame(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event) {
        assert(FolderPtr);
        bool const isActiveFolder =
            (FolderPtr == FolderBrowser.GetFolderView(FolderBrowser.GetActiveFolderIndex(guiSys)));
        if (!isActiveFolder) {
            TouchDown = false;
        }

        OvrFolderBrowser::FolderView& folder = *FolderPtr;
        const int numPanels = static_cast<int>(folder.Panels.size());
        eScrollDirectionLockType touchDirLock = FolderBrowser.GetTouchDirectionLock();
        eScrollDirectionLockType conrollerDirLock = FolderBrowser.GetControllerDirectionLock();

        ScrollMgr.SetMaxPosition(static_cast<float>(numPanels - 1));
        ScrollMgr.SetRestrictedScrollingData(
            FolderBrowser.GetNumFolders() > 1, touchDirLock, conrollerDirLock);

        unsigned int controllerInput = 0;
        if (isActiveFolder) {
            controllerInput = vrFrame.AllButtons;
            bool restrictedScrolling = FolderBrowser.GetNumFolders() > 1;
            if (restrictedScrolling) {
                if (touchDirLock == VERTICAL_LOCK) {
                    if (ScrollMgr.IsTouchIsDown()) {
                        // Restricted scrolling enabled, but locked to vertical scrolling so lose
                        // the touch input
                        ScrollMgr.TouchUp();
                    }
                }

                if (conrollerDirLock != HORIZONTAL_LOCK) {
                    // Restricted scrolling enabled, but not locked to horizontal scrolling so lose
                    // the controller input
                    controllerInput = 0;
                }
            }
        } else {
            if (ScrollMgr.IsTouchIsDown()) {
                // While touch down this specific folder became inactive so perform touch up on this
                // folder.
                ScrollMgr.TouchUp();
            }
        }

        ScrollMgr.Frame(vrFrame.DeltaSeconds, controllerInput);

        if (numPanels <= 0) {
            return MSG_STATUS_ALIVE;
        }

        const float position = ScrollMgr.GetPosition();

        // Send the position to the ScrollBar
        VRMenuObject* scrollBarObject = guiSys.GetVRMenuMgr().ToObject(folder.ScrollBarHandle);
        assert(scrollBarObject != NULL);
        if (isActiveFolder) {
            bool isHidden = false;
            if (HIDE_SCROLLBAR_UNTIL_ITEM_COUNT > 0) {
                isHidden =
                    ScrollMgr.GetMaxPosition() - (float)(HIDE_SCROLLBAR_UNTIL_ITEM_COUNT - 1) <
                    MATH_FLOAT_SMALLEST_NON_DENORMAL;
            }
            scrollBarObject->SetVisible(!isHidden);
        }
        OvrScrollBarComponent* scrollBar =
            scrollBarObject->GetComponentByTypeName<OvrScrollBarComponent>();
        if (scrollBar != NULL) {
            scrollBar->SetScrollFrac(guiSys.GetVRMenuMgr(), scrollBarObject, position);
        }

        // hide the scrollbar if not active
        const float velocity = ScrollMgr.GetVelocity();
        if ((numPanels > 1 && TouchDown) || fabsf(velocity) >= 0.01f) {
            scrollBar->SetScrollState(scrollBarObject, OvrScrollBarComponent::SCROLL_STATE_FADE_IN);
        } else if (!TouchDown && (!isActiveFolder || fabsf(velocity) < 0.01f)) {
            scrollBar->SetScrollState(
                scrollBarObject, OvrScrollBarComponent::SCROLL_STATE_FADE_OUT);
        }

        const float curRot =
            position * (MATH_FLOAT_TWOPI / FolderBrowser.GetCircumferencePanelSlots());
        Quatf rot(UP, curRot);
        rot.Normalize();
        self->SetLocalRotation(rot);

        // show or hide panels based on current position
        //
        // for rendering, we want the switch to occur between panels - hence nearbyint
        const int curPanelIndex = CurrentPanelIndex();
        const int extraPanels = FolderBrowser.GetNumSwipePanels() / 2;
        for (int i = 0; i < numPanels; ++i) {
            OvrFolderBrowser::PanelView* panel = folder.Panels.at(i);
            assert(panel->Handle.IsValid());
            VRMenuObject* panelObject = guiSys.GetVRMenuMgr().ToObject(panel->Handle);
            assert(panelObject);

            VRMenuObjectFlags_t flags = panelObject->GetFlags();
            if (i >= curPanelIndex - extraPanels && i <= curPanelIndex + extraPanels) {
                flags &=
                    ~(VRMenuObjectFlags_t(VRMENUOBJECT_DONT_RENDER) | VRMENUOBJECT_DONT_HIT_ALL);

                if (!panel->Visible && folder.Visible) {
                    panel->Visible = true;
                    const OvrPanel_OnUp* panelUpComp =
                        panelObject->GetComponentById<OvrPanel_OnUp>();
                    if (panelUpComp) {
                        const OvrMetaDatum* panelData = panelUpComp->GetData();
                        assert(panelData);
                        FolderBrowser.QueueAsyncThumbnailLoad(
                            panelData, folder.FolderIndex, panel->Id);
                    }
                }

                panelObject->SetFadeDirection(Vector3f(0.0f));
                if (i == curPanelIndex - extraPanels) {
                    panelObject->SetFadeDirection(-RIGHT);
                } else if (i == curPanelIndex + extraPanels) {
                    panelObject->SetFadeDirection(RIGHT);
                }
            } else {
                flags |= VRMenuObjectFlags_t(VRMENUOBJECT_DONT_RENDER) | VRMENUOBJECT_DONT_HIT_ALL;
                if (panel->Visible) {
                    panel->Visible = false;
                    panel->LoadDefaultThumbnail(
                        guiSys,
                        FolderBrowser.GetDefaultThumbnailTextureId(),
                        FolderBrowser.GetThumbWidth(),
                        FolderBrowser.GetThumbHeight());
                }
            }
            panelObject->SetFlags(flags);
        }

        return MSG_STATUS_ALIVE;
    }

    eMsgStatus OnEvent_Impl(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event) {
        switch (event.EventType) {
            case VRMENU_EVENT_FRAME_UPDATE:
                return Frame(guiSys, vrFrame, self, event);
            case VRMENU_EVENT_TOUCH_DOWN:
                return OnTouchDown_Impl(guiSys, vrFrame, self, event);
            case VRMENU_EVENT_TOUCH_UP:
                return OnTouchUp_Impl(guiSys, vrFrame, self, event);
            case VRMENU_EVENT_SWIPE_COMPLETE: {
                VRMenuEvent upEvent = event;
                upEvent.EventType = VRMENU_EVENT_TOUCH_UP;
                return OnTouchUp_Impl(guiSys, vrFrame, self, upEvent);
            }
            case VRMENU_EVENT_TOUCH_RELATIVE:
                ScrollMgr.TouchRelative(event.FloatValue, vrFrame.RealTimeInSeconds);
                break;
            default:
                assert(!"Event flags mismatch!"); // the constructor is specifying a flag that's not
                                                  // handled
        }

        return MSG_STATUS_ALIVE;
    }

    eMsgStatus OnTouchDown_Impl(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event) {
        ScrollMgr.TouchDown();
        TouchDown = true;
        return MSG_STATUS_ALIVE;
    }

    eMsgStatus OnTouchUp_Impl(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event) {
        ScrollMgr.TouchUp();
        TouchDown = false;
        return MSG_STATUS_ALIVE;
    }

   private:
    OvrFolderBrowser& FolderBrowser;
    OvrScrollManager ScrollMgr;
    OvrFolderBrowser::FolderView*
        FolderPtr; // Correlates the folder browser component to the folder it belongs to
    bool TouchDown;
};

//==============================
// OvrFolderBrowser
unsigned char* OvrFolderBrowser::ThumbPanelBG = NULL;

OvrFolderBrowser::OvrFolderBrowser(
    OvrGuiSys& guiSys,
    OvrMetaData& metaData,
    float panelWidth,
    float panelHeight,
    float radius_,
    unsigned numSwipePanels,
    unsigned thumbWidth,
    unsigned thumbHeight)
    : VRMenu(MENU_NAME),
      MediaCount(0),
      PanelWidth(0.0f),
      PanelHeight(0.0f),
      ThumbWidth(thumbWidth),
      ThumbHeight(thumbHeight),
      PanelTextSpacingScale(0.5f),
      FolderTitleSpacingScale(0.5f),
      ScrollBarSpacingScale(0.5f),
      ScrollBarRadiusScale(0.97f),
      NumSwipePanels(numSwipePanels),
      NoMedia(false),
      AllowPanelTouchUp(false),
      TextureCommands(10000),
      BackgroundCommands(10000),
      ControllerDirectionLock(NO_LOCK),
      LastControllerInputTimeStamp(0.0f),
      IsTouchDownPosistionTracked(false),
      TouchDirectionLocked(NO_LOCK) {
    //  Load up thumbnail alpha from panel.tga
    if (ThumbPanelBG == NULL) {
        void* buffer;
        int bufferLength;

        const char* panel = NULL;

        if (ThumbWidth == ThumbHeight) {
            panel = "res/raw/panel_square.tga";
        } else {
            panel = "res/raw/panel.tga";
        }

        ovr_ReadFileFromApplicationPackage(panel, bufferLength, buffer);

        int panelW = 0;
        int panelH = 0;
        ThumbPanelBG =
            stbi_load_from_memory((stbi_uc const*)buffer, bufferLength, &panelW, &panelH, NULL, 4);

        assert(ThumbPanelBG != 0 && panelW == ThumbWidth && panelH == ThumbHeight);
    }

    // load up the default panel textures once
    {
        const char* panelSrc[2] = {};

        if (ThumbWidth == ThumbHeight) {
            panelSrc[0] = "apk:///res/raw/panel_square.tga";
            panelSrc[1] = "apk:///res/raw/panel_hi_square.tga";
        } else {
            panelSrc[0] = "apk:///res/raw/panel.tga";
            panelSrc[1] = "apk:///res/raw/panel_hi.tga";
        }

        for (int t = 0; t < 2; ++t) {
            int width = 0;
            int height = 0;
            DefaultPanelTextureIds[t] = LoadTextureFromUri(
                guiSys.GetFileSys(),
                panelSrc[t],
                TextureFlags_t(TEXTUREFLAG_NO_DEFAULT),
                width,
                height);
            assert(DefaultPanelTextureIds[t] && (width == ThumbWidth) && (height == ThumbHeight));

            BuildTextureMipmaps(DefaultPanelTextureIds[t]);
            MakeTextureTrilinear(DefaultPanelTextureIds[t]);
            MakeTextureClamped(DefaultPanelTextureIds[t]);
        }
    }

    ThumbnailLoadingThread = std::thread(&OvrFolderBrowser::ThumbnailThread, this);

    PanelWidth = panelWidth * VRMenuObject::DEFAULT_TEXEL_SCALE;
    PanelHeight = panelHeight * VRMenuObject::DEFAULT_TEXEL_SCALE;
    Radius = radius_;
    const float circumference = MATH_FLOAT_TWOPI * Radius;

    CircumferencePanelSlots = (int)(floor(circumference / PanelWidth));
    VisiblePanelsArcAngle =
        ((float)(NumSwipePanels + 1) / CircumferencePanelSlots) * MATH_FLOAT_TWOPI;

    std::vector<VRMenuComponent*> comps;
    comps.push_back(new OvrFolderBrowserRootComponent(*this));
    Init(guiSys, 0.0f, VRMenuFlags_t(), comps);
}

VRMenuComponent* OvrFolderBrowser::CreateFolderBrowserSwipeComponent(
    OvrFolderBrowser& folderBrowser,
    OvrFolderBrowser::FolderView* folder) {
    return new OvrFolderSwipeComponent(*this, folder);
}

OvrFolderBrowser::~OvrFolderBrowser() {
    ALOG("OvrFolderBrowser::~OvrFolderBrowser");
    // Wake up thumbnail thread if needed and shut it down
    ThumbnailThreadState.store(THUMBNAIL_THREAD_SHUTDOWN);
    ThumbnailThreadCondition.notify_all();

    ALOG("OvrFolderBrowser::~OvrFolderBrowser - shutting down msg queue ...");
    BackgroundCommands.Shutdown();

    if (ThumbnailLoadingThread.joinable()) {
        ALOG("OvrFolderBrowser::~OvrFolderBrowser - joining ThumbnailLoadingThread ...");
        ThumbnailLoadingThread.join();
    }

    for (FolderView* folder : Folders) {
        if (folder) {
            folder->FreeThumbnailTextures(DefaultPanelTextureIds[0]);
            delete folder;
        }
    }

    if (ThumbPanelBG != NULL) {
        free(ThumbPanelBG);
        ThumbPanelBG = NULL;
    }

    for (int t = 0; t < 2; ++t) {
        DeleteTexture(DefaultPanelTextureIds[t]);
    }

    ALOG("OvrFolderBrowser::~OvrFolderBrowser COMPLETE");
}

void OvrFolderBrowser::Frame_Impl(OvrGuiSys& guiSys, ovrApplFrameIn const& vrFrame) {
    // Check for thumbnail loads
    while (1) {
        const char* cmd = TextureCommands.GetNextMessage();
        if (!cmd) {
            break;
        }

        // ALOG( "TextureCommands: %s", cmd );
        LoadThumbnailToTexture(guiSys, cmd);
        free((void*)cmd);
    }

    // --
    // Logic for restricted scrolling
    unsigned int controllerInput = vrFrame.AllButtons;
    bool rightPressed = (controllerInput & (ovrButton_Right)) != 0;
    bool leftPressed = (controllerInput & (ovrButton_Left)) != 0;
    bool downPressed = (controllerInput & (ovrButton_Down)) != 0;
    bool upPressed = (controllerInput & (ovrButton_Up)) != 0;

    if (!(rightPressed || leftPressed || downPressed || upPressed)) {
        if (ControllerDirectionLock != NO_LOCK) {
            if (vrapi_GetTimeInSeconds() - LastControllerInputTimeStamp >= CONTROLER_COOL_DOWN) {
                // Didn't receive any input for last 'CONTROLER_COOL_DOWN' seconds, so release
                // direction lock
                ControllerDirectionLock = NO_LOCK;
            }
        }
    } else {
        if (rightPressed || leftPressed) {
            if (ControllerDirectionLock == VERTICAL_LOCK) {
                rightPressed = false;
                leftPressed = false;
            } else {
                if (ControllerDirectionLock != HORIZONTAL_LOCK) {
                    ControllerDirectionLock = HORIZONTAL_LOCK;
                }
                LastControllerInputTimeStamp = static_cast<float>(vrapi_GetTimeInSeconds());
            }
        }

        if (downPressed || upPressed) {
            if (ControllerDirectionLock == HORIZONTAL_LOCK) {
                downPressed = false;
                upPressed = false;
            } else {
                if (ControllerDirectionLock != VERTICAL_LOCK) {
                    ControllerDirectionLock = VERTICAL_LOCK;
                }
                LastControllerInputTimeStamp = static_cast<float>(vrapi_GetTimeInSeconds());
            }
        }
    }
}

VRMenuId_t OvrFolderBrowser::NextId() {
    return VRMenuId_t(uniqueId.Get(1));
}

void OvrFolderBrowser::Open_Impl(OvrGuiSys& guiSys) {
    if (NoMedia) {
        return;
    }

    // Rebuild favorites if not empty
    OnBrowserOpen(guiSys);

    // Wake up thumbnail thread
    ThumbnailThreadState.store(THUMBNAIL_THREAD_WORK);
    ThumbnailThreadCondition.notify_all();
}

void OvrFolderBrowser::Close_Impl(OvrGuiSys& guiSys) {
    ThumbnailThreadState.store(THUMBNAIL_THREAD_PAUSE);
}

void OvrFolderBrowser::OneTimeInit(OvrGuiSys& guiSys) {
    /// TODO - validate
    const ovrJava& java = *reinterpret_cast<const ovrJava*>(guiSys.GetContext()->ContextForVrApi());
    ovrFileSys::GetPathIfValidPermission(
        java,
        EST_PRIMARY_EXTERNAL_STORAGE,
        EFT_CACHE,
        "",
        permissionFlags_t(PERMISSION_WRITE),
        AppCachePath);
    assert(!AppCachePath.empty());

    ovrFileSys::PushBackSearchPathIfValid(
        java, EST_SECONDARY_EXTERNAL_STORAGE, EFT_ROOT, "RetailMedia/", ThumbSearchPaths);
    ovrFileSys::PushBackSearchPathIfValid(
        java, EST_SECONDARY_EXTERNAL_STORAGE, EFT_ROOT, "", ThumbSearchPaths);
    ovrFileSys::PushBackSearchPathIfValid(
        java, EST_PRIMARY_EXTERNAL_STORAGE, EFT_ROOT, "RetailMedia/", ThumbSearchPaths);
    ovrFileSys::PushBackSearchPathIfValid(
        java, EST_PRIMARY_EXTERNAL_STORAGE, EFT_ROOT, "", ThumbSearchPaths);
    assert(!ThumbSearchPaths.empty());

    // move the root up to eye height
    OvrVRMenuMgr& menuManager = guiSys.GetVRMenuMgr();
    VRMenuObject* root = menuManager.ToObject(GetRootHandle());
    assert(root);
    if (root != NULL) {
        Vector3f pos = root->GetLocalPosition();
        pos.y += EYE_HEIGHT_OFFSET;
        root->SetLocalPosition(pos);
    }

    std::vector<VRMenuComponent*> comps;
    std::vector<VRMenuObjectParms const*> parms;

    // Folders root folder
    FoldersRootId = VRMenuId_t(uniqueId.Get(1));
    VRMenuObjectParms foldersRootParms(
        VRMENU_CONTAINER,
        comps,
        VRMenuSurfaceParms(),
        "Folder Browser Folders",
        Posef(),
        Vector3f(1.0f),
        VRMenuFontParms(),
        FoldersRootId,
        VRMenuObjectFlags_t(),
        VRMenuObjectInitFlags_t());
    parms.push_back(&foldersRootParms);
    AddItems(guiSys, parms, GetRootHandle(), false);
    parms.clear();
    comps.clear();

    // Build scroll up/down hints attached to root
    VRMenuId_t scrollSuggestionRootId(uniqueId.Get(1));

    VRMenuObjectParms scrollSuggestionParms(
        VRMENU_CONTAINER,
        comps,
        VRMenuSurfaceParms(),
        "scroll hints",
        Posef(),
        Vector3f(1.0f),
        VRMenuFontParms(),
        scrollSuggestionRootId,
        VRMenuObjectFlags_t(),
        VRMenuObjectInitFlags_t());
    parms.push_back(&scrollSuggestionParms);
    AddItems(guiSys, parms, GetRootHandle(), false);
    parms.clear();

    ScrollSuggestionRootHandle = root->ChildHandleForId(menuManager, scrollSuggestionRootId);
    assert(ScrollSuggestionRootHandle.IsValid());

    VRMenuId_t suggestionDownId(uniqueId.Get(1));
    VRMenuId_t suggestionUpId(uniqueId.Get(1));

    const Posef swipeDownPose(Quatf(), FWD * (0.33f * Radius) + DOWN * PanelHeight * 0.5f);
    menuHandle_t scrollDownHintHandle = OvrSwipeHintComponent::CreateSwipeSuggestionIndicator(
        guiSys,
        this,
        ScrollSuggestionRootHandle,
        static_cast<int>(suggestionDownId.Get()),
        "res/raw/swipe_suggestion_arrow_down.png",
        swipeDownPose,
        DOWN);

    const Posef swipeUpPose(Quatf(), FWD * (0.33f * Radius) + UP * PanelHeight * 0.5f);
    menuHandle_t scrollUpHintHandle = OvrSwipeHintComponent::CreateSwipeSuggestionIndicator(
        guiSys,
        this,
        ScrollSuggestionRootHandle,
        static_cast<int>(suggestionUpId.Get()),
        "res/raw/swipe_suggestion_arrow_up.png",
        swipeUpPose,
        UP);

    OvrFolderBrowserRootComponent* rootComp =
        root->GetComponentById<OvrFolderBrowserRootComponent>();
    assert(rootComp);

    menuHandle_t foldersRootHandle = root->ChildHandleForId(menuManager, FoldersRootId);
    assert(foldersRootHandle.IsValid());
    rootComp->SetFoldersRootHandle(foldersRootHandle);

    assert(scrollUpHintHandle.IsValid());
    rootComp->SetScrollDownHintHandle(scrollDownHintHandle);

    assert(scrollDownHintHandle.IsValid());
    rootComp->SetScrollUpHintHandle(scrollUpHintHandle);
}

void OvrFolderBrowser::BuildDirtyMenu(OvrGuiSys& guiSys, OvrMetaData& metaData) {
    OvrVRMenuMgr& menuManager = guiSys.GetVRMenuMgr();

    std::vector<VRMenuComponent*> comps;
    std::vector<const VRMenuObjectParms*> parms;

    const std::vector<OvrMetaData::Category>& categories = metaData.GetCategories();
    const int numCategories = static_cast<int>(categories.size());

    // load folders and position
    for (int catIndex = 0; catIndex < numCategories; ++catIndex) {
        // Load a category to build swipe folder
        OvrMetaData::Category& currentCategory = metaData.GetCategory(catIndex);
        if (currentCategory.Dirty) // Only build if dirty
        {
            ALOG("Loading folder %i named %s", catIndex, currentCategory.CategoryTag.c_str());
            FolderView* folder = GetFolderView(currentCategory.CategoryTag);

            // if building for the first time
            if (folder == NULL) {
                // Create internal folder struct
                std::string localizedCategoryName;

                // Get localized tag (folder title)
                localizedCategoryName = GetCategoryTitle(
                    guiSys, currentCategory.CategoryTag.c_str(), currentCategory.LocaleKey.c_str());

                folder = CreateFolderView(localizedCategoryName, currentCategory.CategoryTag);
                Folders.push_back(folder);

                const int folderIndex = static_cast<int>(Folders.size()) -
                    1; // Can't rely on catIndex due to duplicate folders

                BuildFolderView(
                    guiSys, currentCategory, folder, metaData, FoldersRootId, folderIndex);

                UpdateFolderTitle(guiSys, folder);
                folder->MaxRotation = CalcFolderMaxRotation(folder);
            } else // already have this folder - rebuild it with the new metadata
            {
                // Find the index we already have
                int existingIndex = 0;
                for (; existingIndex < numCategories; ++existingIndex) {
                    // Load the category to build swipe folder
                    const OvrMetaData::Category& cat = metaData.GetCategory(existingIndex);
                    if (cat.CategoryTag == currentCategory.CategoryTag) {
                        std::vector<const OvrMetaDatum*> folderMetaData;
                        if (metaData.GetMetaData(cat, folderMetaData)) {
                            RebuildFolderView(guiSys, metaData, existingIndex, folderMetaData);
                        } else {
                            ALOG(
                                "Failed to get any metaData for folder %i named %s",
                                existingIndex,
                                currentCategory.CategoryTag.c_str());
                        }
                        break;
                    }
                }
            }

            folder->FolderIndex = catIndex;

            currentCategory.Dirty = false;

            // Set up initial positions - 0 in center, the rest ascending in order below it
            MediaCount += static_cast<int>(folder->Panels.size());
            VRMenuObject* folderObject = menuManager.ToObject(folder->Handle);
            assert(folderObject != NULL);
            folderObject->SetLocalPosition(
                (DOWN * PanelHeight * static_cast<float>(catIndex)) +
                folderObject->GetLocalPosition());
        }
    }

    const VRMenuFontParms fontParms(
        HORIZONTAL_CENTER, VERTICAL_CENTER, false, false, true, 0.525f, 0.45f, 0.5f);

    // Show no media menu if no media found
    if (MediaCount == 0) {
        std::string title;
        std::string imageFile;
        std::string message;
        OnMediaNotFound(guiSys, title, imageFile, message);

        // Create a folder if we didn't create at least one to display no media info
        if (Folders.size() < 1) {
            const std::string noMediaTag("No Media");
            const_cast<OvrMetaData&>(metaData).AddCategory(noMediaTag);
            OvrMetaData::Category& noMediaCategory = metaData.GetCategory(0);
            FolderView* noMediaView = new FolderView(noMediaTag, noMediaTag);
            BuildFolderView(guiSys, noMediaCategory, noMediaView, metaData, FoldersRootId, 0);
            Folders.push_back(noMediaView);
        }

        // Set title
        const FolderView* folder = GetFolderView(0);
        assert(folder != NULL);
        VRMenuObject* folderTitleObject = menuManager.ToObject(folder->TitleHandle);
        assert(folderTitleObject != NULL);
        folderTitleObject->SetText(title.c_str());
        folderTitleObject->SetVisible(true);

        // Add no media panel
        const Vector3f dir(FWD);
        const Posef panelPose(Quatf(), dir * Radius);
        const Vector3f panelScale(1.0f);
        const Posef textPose(Quatf(), Vector3f(0.0f, -0.3f, 0.0f));

        VRMenuSurfaceParms panelSurfParms(
            "panelSurface",
            imageFile.c_str(),
            SURFACE_TEXTURE_DIFFUSE,
            NULL,
            SURFACE_TEXTURE_MAX,
            NULL,
            SURFACE_TEXTURE_MAX);

        VRMenuObjectParms* p = new VRMenuObjectParms(
            VRMENU_STATIC,
            std::vector<VRMenuComponent*>(),
            panelSurfParms,
            message.c_str(),
            panelPose,
            panelScale,
            textPose,
            Vector3f(1.3f),
            fontParms,
            VRMenuId_t(),
            VRMenuObjectFlags_t(VRMENUOBJECT_DONT_HIT_ALL),
            VRMenuObjectInitFlags_t(VRMENUOBJECT_INIT_FORCE_POSITION));

        parms.push_back(p);
        AddItems(guiSys, parms, folder->SwipeHandle, true); // PARENT: folder.TitleRootHandle
        parms.clear();

        NoMedia = true;

        // Hide scroll hints while no media
        VRMenuObject* scrollHintRootObject = menuManager.ToObject(ScrollSuggestionRootHandle);
        assert(scrollHintRootObject);
        scrollHintRootObject->SetVisible(false);
    }
}

void OvrFolderBrowser::BuildFolderView(
    OvrGuiSys& guiSys,
    OvrMetaData::Category& category,
    FolderView* const folder,
    const OvrMetaData& metaData,
    VRMenuId_t foldersRootId,
    int folderIndex) {
    assert(folder);

    OvrVRMenuMgr& menuManager = guiSys.GetVRMenuMgr();

    std::vector<const VRMenuObjectParms*> parms;
    const VRMenuFontParms fontParms(
        HORIZONTAL_CENTER, VERTICAL_CENTER, false, false, true, 0.525f, 0.45f, 1.0f);

    const int numPanels = static_cast<int>(category.DatumIndicies.size());

    VRMenuObject* root = menuManager.ToObject(GetRootHandle());
    assert(root != NULL);
    menuHandle_t foldersRootHandle = root->ChildHandleForId(menuManager, foldersRootId);

    // Create OvrFolderRootComponent for folder root
    const VRMenuId_t folderId(uniqueId.Get(1));
    ALOG(
        "Building Folder %s id: %lld with %d panels",
        category.CategoryTag.c_str(),
        folderId.Get(),
        numPanels);
    std::vector<VRMenuComponent*> comps;
    comps.push_back(new OvrFolderRootComponent(*this, folder));
    VRMenuObjectParms folderParms(
        VRMENU_CONTAINER,
        comps,
        VRMenuSurfaceParms(),
        (folder->LocalizedName + " root").c_str(),
        Posef(),
        Vector3f(1.0f),
        fontParms,
        folderId,
        VRMenuObjectFlags_t(),
        VRMenuObjectInitFlags_t(VRMENUOBJECT_INIT_FORCE_POSITION));
    parms.push_back(&folderParms);
    AddItems(guiSys, parms, foldersRootHandle, false); // PARENT: Root
    parms.clear();

    // grab the folder handle and make sure it was created
    folder->Handle = HandleForId(menuManager, folderId);
    VRMenuObject* folderObject = menuManager.ToObject(folder->Handle);
    assert(folderObject != NULL);

    // Add horizontal scrollbar to folder
    Posef scrollBarPose(Quatf(), FWD * Radius * ScrollBarRadiusScale);

    // Create unique ids for the scrollbar objects
    const VRMenuId_t scrollRootId(uniqueId.Get(1));
    const VRMenuId_t scrollXFormId(uniqueId.Get(1));
    const VRMenuId_t scrollBaseId(uniqueId.Get(1));
    const VRMenuId_t scrollThumbId(uniqueId.Get(1));

    // Set the border of the thumb image for 9-slicing
    const Vector4f scrollThumbBorder(0.0f, 0.0f, 0.0f, 0.0f);
    const Vector3f xFormPos = DOWN * static_cast<float>(ThumbHeight) *
        VRMenuObject::DEFAULT_TEXEL_SCALE * ScrollBarSpacingScale;

    // Build the scrollbar
    OvrScrollBarComponent::GetScrollBarParms(
        guiSys,
        *this,
        SCROLL_BAR_LENGTH,
        folderId,
        scrollRootId,
        scrollXFormId,
        scrollBaseId,
        scrollThumbId,
        scrollBarPose,
        Posef(Quatf(), xFormPos),
        0,
        numPanels,
        false,
        scrollThumbBorder,
        parms);
    AddItems(guiSys, parms, folder->Handle, false); // PARENT: folder->Handle
    parms.clear();

    // Cache off the handle and verify successful creation
    folder->ScrollBarHandle = folderObject->ChildHandleForId(menuManager, scrollRootId);
    VRMenuObject* scrollBarObject = menuManager.ToObject(folder->ScrollBarHandle);
    assert(scrollBarObject != NULL);
    OvrScrollBarComponent* scrollBar =
        scrollBarObject->GetComponentByTypeName<OvrScrollBarComponent>();
    if (scrollBar != NULL) {
        scrollBar->UpdateScrollBar(menuManager, scrollBarObject, numPanels);
        scrollBar->SetScrollFrac(menuManager, scrollBarObject, 0.0f);
        scrollBar->SetBaseColor(menuManager, scrollBarObject, Vector4f(1.0f, 1.0f, 1.0f, 1.0f));

        // Hide the scrollbar
        VRMenuObjectFlags_t flags = scrollBarObject->GetFlags();
        scrollBarObject->SetFlags(flags |= VRMenuObjectFlags_t(VRMENUOBJECT_DONT_RENDER));
    }

    // Add OvrFolderSwipeComponent to folder - manages panel swiping
    VRMenuId_t swipeFolderId(uniqueId.Get(1));
    std::vector<VRMenuComponent*> swipeComps;
    swipeComps.push_back(CreateFolderBrowserSwipeComponent(*this, folder));
    VRMenuObjectParms swipeParms(
        VRMENU_CONTAINER,
        swipeComps,
        VRMenuSurfaceParms(),
        (folder->LocalizedName + " swipe").c_str(),
        Posef(),
        Vector3f(1.0f),
        fontParms,
        swipeFolderId,
        VRMenuObjectFlags_t(VRMENUOBJECT_NO_GAZE_HILIGHT),
        VRMenuObjectInitFlags_t(VRMENUOBJECT_INIT_FORCE_POSITION));
    parms.push_back(&swipeParms);
    AddItems(guiSys, parms, folder->Handle, false); // PARENT: folder->Handle
    parms.clear();

    // grab the SwipeHandle handle and make sure it was created
    folder->SwipeHandle = folderObject->ChildHandleForId(menuManager, swipeFolderId);
    VRMenuObject* swipeObject = menuManager.ToObject(folder->SwipeHandle);
    assert(swipeObject != NULL);

    // build a collision primitive that encompasses all of the panels for a raw (including the empty
    // space between them) so that we can always send our swipe messages to the correct row based on
    // gaze.
    std::vector<Vector3f> vertices(CircumferencePanelSlots * 2);
    std::vector<TriangleIndex> indices(CircumferencePanelSlots * 6);
    int curIndex = 0;
    TriangleIndex curVertex = 0;
    for (int i = 0; i < CircumferencePanelSlots; ++i) {
        float theta = (i * MATH_FLOAT_TWOPI) / CircumferencePanelSlots;
        float x = cosf(theta) * Radius * 1.05f;
        float z = sinf(theta) * Radius * 1.05f;
        Vector3f topVert(x, PanelHeight * 0.5f, z);
        Vector3f bottomVert(x, PanelHeight * -0.5f, z);

        vertices[curVertex + 0] = topVert;
        vertices[curVertex + 1] = bottomVert;
        if (i > 0) // only set up indices if we already have one column's vertices
        {
            // first tri
            indices[curIndex + 0] = curVertex + 1;
            indices[curIndex + 1] = curVertex + 0;
            indices[curIndex + 2] = curVertex - 1;
            // second tri
            indices[curIndex + 3] = curVertex + 0;
            indices[curIndex + 4] = curVertex - 2;
            indices[curIndex + 5] = curVertex - 1;
            curIndex += 6;
        }

        curVertex += 2;
    }
    // connect the last vertices to the first vertices for the last sector
    indices[curIndex + 0] = 1;
    indices[curIndex + 1] = 0;
    indices[curIndex + 2] = curVertex - 1;
    indices[curIndex + 3] = 0;
    indices[curIndex + 4] = curVertex - 2;
    indices[curIndex + 5] = curVertex - 1;

    // create and set the collision primitive on the swipe object
    OvrTriCollisionPrimitive* cp = new OvrTriCollisionPrimitive(
        vertices, indices, std::vector<Vector2f>(), ContentFlags_t(CONTENT_SOLID));
    swipeObject->SetCollisionPrimitive(cp);

    if (!category.DatumIndicies.empty()) {
        // Grab panel parms
        LoadFolderViewPanels(guiSys, metaData, category, folderIndex, *folder, parms);
        if (!parms.empty()) {
            // Add panels to swipehandle
            AddItems(guiSys, parms, folder->SwipeHandle, false);
            DeletePointerArray(parms);
            parms.clear();
        }

        // Assign handles to panels
        for (PanelView* panel : folder->Panels) {
            panel->Handle = swipeObject->ChildHandleForId(menuManager, panel->MenuId);
        }
    }

    // Folder title
    VRMenuId_t folderTitleRootId(uniqueId.Get(1));
    VRMenuObjectParms titleRootParms(
        VRMENU_CONTAINER,
        std::vector<VRMenuComponent*>(),
        VRMenuSurfaceParms(),
        (folder->LocalizedName + " title root").c_str(),
        Posef(),
        Vector3f(1.0f),
        fontParms,
        folderTitleRootId,
        VRMenuObjectFlags_t(),
        VRMenuObjectInitFlags_t(VRMENUOBJECT_INIT_FORCE_POSITION));
    parms.push_back(&titleRootParms);
    AddItems(guiSys, parms, folder->Handle, false); // PARENT: folder->Handle
    parms.clear();

    // grab the title root handle and make sure it was created
    folder->TitleRootHandle = folderObject->ChildHandleForId(menuManager, folderTitleRootId);
    VRMenuObject* folderTitleRootObject = menuManager.ToObject(folder->TitleRootHandle);
    OVR_UNUSED(folderTitleRootObject);
    assert(folderTitleRootObject != NULL);

    VRMenuId_t folderTitleId(uniqueId.Get(1));
    Posef titlePose(Quatf(), FWD * Radius + UP * PanelHeight * FolderTitleSpacingScale);
    VRMenuObjectParms titleParms(
        VRMENU_STATIC,
        std::vector<VRMenuComponent*>(),
        VRMenuSurfaceParms(),
        "no title",
        titlePose,
        Vector3f(1.25f),
        fontParms,
        folderTitleId,
        VRMenuObjectFlags_t(VRMENUOBJECT_DONT_HIT_TEXT),
        VRMenuObjectInitFlags_t(VRMENUOBJECT_INIT_FORCE_POSITION));
    parms.push_back(&titleParms);
    AddItems(guiSys, parms, folder->TitleRootHandle, true); // PARENT: folder->TitleRootHandle
    parms.clear();

    // grab folder title handle and make sure it was created
    folder->TitleHandle = folderTitleRootObject->ChildHandleForId(menuManager, folderTitleId);
    VRMenuObject* folderTitleObject = menuManager.ToObject(folder->TitleHandle);
    OVR_UNUSED(folderTitleObject);
    assert(folderTitleObject != NULL);
}

void OvrFolderBrowser::RebuildFolderView(
    OvrGuiSys& guiSys,
    OvrMetaData& metaData,
    const int folderIndex,
    const std::vector<const OvrMetaDatum*>& data) {
    if (folderIndex >= 0 && static_cast<int>(Folders.size()) > folderIndex) {
        OvrVRMenuMgr& menuManager = guiSys.GetVRMenuMgr();

        FolderView* folder = GetFolderView(folderIndex);
        if (folder == NULL) {
            ALOG(
                "OvrFolderBrowser::RebuildFolder failed to Folder for folderIndex %d", folderIndex);
            return;
        }

        VRMenuObject* swipeObject = menuManager.ToObject(folder->SwipeHandle);
        assert(swipeObject);

        swipeObject->FreeChildren(menuManager);
        folder->Panels.clear();

        const int numPanels = static_cast<int>(data.size());
        std::vector<const VRMenuObjectParms*> outParms;
        std::vector<int> newDatumIndicies;
        for (int panelIndex = 0; panelIndex < numPanels; ++panelIndex) {
            const OvrMetaDatum* panelData = data.at(panelIndex);
            if (panelData) {
                AddPanelToFolder(guiSys, data.at(panelIndex), folderIndex, *folder, outParms);
                if (!outParms.empty()) {
                    AddItems(guiSys, outParms, folder->SwipeHandle, false);
                    DeletePointerArray(outParms);
                    outParms.clear();
                }

                // Assign handle to panel
                PanelView* panel = folder->Panels.at(panelIndex);
                panel->Handle = swipeObject->ChildHandleForId(menuManager, panel->MenuId);
                newDatumIndicies.push_back(panelData->Id);
            }
        }

        metaData.SetCategoryDatumIndicies(folderIndex, newDatumIndicies);

        OvrFolderSwipeComponent* swipeComp =
            swipeObject->GetComponentById<OvrFolderSwipeComponent>();
        assert(swipeComp);
        UpdateFolderTitle(guiSys, folder);

        // Recalculate accumulated rotation in the swipe component based on ratio of where user left
        // off before adding/removing favorites
        const float currentMaxRotation = folder->MaxRotation > 0.0f ? folder->MaxRotation : 1.0f;
        const float positionRatio =
            clamp<float>(swipeComp->GetAccumulatedRotation() / currentMaxRotation, 0.0f, 1.0f);
        folder->MaxRotation = CalcFolderMaxRotation(folder);
        swipeComp->SetAccumulatedRotation(folder->MaxRotation * positionRatio);

        // Update the scroll bar on new element count
        VRMenuObject* scrollBarObject = menuManager.ToObject(folder->ScrollBarHandle);
        if (scrollBarObject != NULL) {
            OvrScrollBarComponent* scrollBar =
                scrollBarObject->GetComponentByTypeName<OvrScrollBarComponent>();
            if (scrollBar != NULL) {
                scrollBar->UpdateScrollBar(menuManager, scrollBarObject, numPanels);
            }
        }
    }
}

void OvrFolderBrowser::UpdateFolderTitle(OvrGuiSys& guiSys, const FolderView* folder) {
    if (folder != NULL) {
        const int numPanels = static_cast<int>(folder->Panels.size());

        std::string folderTitle = folder->LocalizedName;
        VRMenuObject* folderTitleObject = guiSys.GetVRMenuMgr().ToObject(folder->TitleHandle);
        assert(folderTitleObject != NULL);
        folderTitleObject->SetText(folderTitle.c_str());

        VRMenuObjectFlags_t flags = folderTitleObject->GetFlags();
        if (numPanels > 0) {
            flags &= ~VRMenuObjectFlags_t(VRMENUOBJECT_DONT_RENDER);
            flags &= ~VRMenuObjectFlags_t(VRMENUOBJECT_DONT_HIT_ALL);
        } else {
            flags |= VRMenuObjectFlags_t(VRMENUOBJECT_DONT_RENDER);
            flags |= VRMenuObjectFlags_t(VRMENUOBJECT_DONT_HIT_ALL);
        }

        folderTitleObject->SetFlags(flags);
    }
}

void OvrFolderBrowser::ThumbnailThread() {
    // thread->SetThreadName( "FolderBrowser" );

    for (;;) {
        BackgroundCommands.SleepUntilMessage();

        switch (ThumbnailThreadState.load()) {
            case THUMBNAIL_THREAD_WORK:
                break;
            case THUMBNAIL_THREAD_PAUSE: {
                ALOG("OvrFolderBrowser::ThumbnailThread PAUSED");
                {
                    while (ThumbnailThreadState.load() == THUMBNAIL_THREAD_PAUSE) {
                        std::unique_lock<std::mutex> lk(ThumbnailThreadMutex);
                        ThumbnailThreadCondition.wait(lk);
                    }
                }
                ALOG("OvrFolderBrowser::ThumbnailThread PAUSED DONE");
                continue;
            }
            case THUMBNAIL_THREAD_SHUTDOWN:
                ALOG("OvrFolderBrowser::ThumbnailThread shutting down");
                return;
        }

        const char* msg = BackgroundCommands.GetNextMessage();
        ALOG("BackgroundCommands: %s", msg);

        if (MatchesHead("load ", msg)) {
            int folderId = -1;
            int panelId = -1;

            sscanf(msg, "load %i %i", &folderId, &panelId);
            assert(folderId >= 0 && panelId >= 0);

            // Do we still need to load this?
            const FolderView* folder = GetFolderView(folderId);
            // ThumbnailsLoaded is set to false when the category goes out of view - do not load the
            // thumbnail
            if (folder && folder->Visible) {
                if (panelId >= 0 && panelId < static_cast<int>(folder->Panels.size())) {
                    const PanelView* panel = folder->Panels.at(panelId);
                    if (panel && panel->Visible) {
                        const char* fileName = strstr(msg, ":") + 1;

                        const std::string fullPath(fileName);

                        int width;
                        int height;
                        unsigned char* data = LoadThumbnail(fileName, width, height);
                        if (data != NULL) {
                            TextureCommands.PostPrintf(
                                "thumb %i %i %p %i %i", folderId, panelId, data, width, height);
                        } else {
                            ALOGW("Thumbnail load fail for: %s", fileName);
                        }
                    }
                }
            }

        } else if (MatchesHead("httpThumb", msg)) {
            int folderId = -1;
            int panelId = -1;
            char panoUrl[1024] = {};
            char cacheDestination[1024] = {};

            sscanf(msg, "httpThumb %s %s %d %d", panoUrl, cacheDestination, &folderId, &panelId);

            // Do we still need to load this?
            const FolderView* folder = GetFolderView(folderId);
            // ThumbnailsLoaded is set to false when the category goes out of view - do not load the
            // thumbnail
            if (folder && folder->Visible) {
                if (panelId >= 0 && panelId < static_cast<int>(folder->Panels.size())) {
                    const PanelView* panel = folder->Panels.at(panelId);
                    if (panel && panel->Visible) {
                        int width;
                        int height;
                        unsigned char* data = RetrieveRemoteThumbnail(
                            panoUrl, cacheDestination, folderId, panelId, width, height);

                        if (data != NULL) {
                            TextureCommands.PostPrintf(
                                "thumb %i %i %p %i %i", folderId, panelId, data, width, height);
                        } else {
                            ALOGW("Thumbnail download fail for: %s", panoUrl);
                        }
                    }
                }
            }
        } else {
            ALOG("OvrFolderBrowser::ThumbnailThread received unhandled message: %s", msg);
            assert(false);
        }

        free((void*)msg);
    }
}

// THUMBFIX: call this to load final thumbnail onto the panel
void OvrFolderBrowser::LoadThumbnailToTexture(OvrGuiSys& guiSys, const char* thumbnailCommand) {
    int folderId;
    int panelId;
    unsigned char* data;
    int width;
    int height;

    sscanf(thumbnailCommand, "thumb %i %i %p %i %i", &folderId, &panelId, &data, &width, &height);
    if (folderId < 0 || panelId < 0) {
        delete data;
        return;
    }

    FolderView* folder = GetFolderView(folderId);
    if (folder == NULL) {
        ALOGW("OvrFolderBrowser::LoadThumbnailToTexture failed to find FolderView at %i", folderId);
        free(data);
        return;
    }

    std::vector<PanelView*>* panels = &folder->Panels;
    if (panels == NULL) {
        ALOGW("OvrFolderBrowser::LoadThumbnailToTexture failed to get panels array from folder");
        free(data);
        return;
    }

    PanelView* panel = NULL;

    // find panel using panelId
    const int numPanels = static_cast<int>(panels->size());
    for (int index = 0; index < numPanels; ++index) {
        PanelView* currentPanel = panels->at(index);
        if (currentPanel->Id == panelId) {
            panel = currentPanel;
            break;
        }
    }

    if (panel == NULL) // Panel not found as it was moved. Delete data and bail
    {
        ALOGW(
            "OvrFolderBrowser::LoadThumbnailToTexture failed to find panel id %d in folder %d",
            panelId,
            folderId);
        free(data);
        return;
    }

    if (!ApplyThumbAntialiasing(data, width, height)) {
        ALOGW(
            "OvrFolderBrowser::LoadThumbnailToTexture Failed to apply AA to %s", thumbnailCommand);
    }

    // Grab the Panel from VRMenu
    menuHandle_t thumbHandle = panel->GetThumbnailHandle();
    VRMenuObject* panelObject = guiSys.GetVRMenuMgr().ToObject(thumbHandle);
    assert(panelObject);

    GlTexture texId = LoadRGBATextureFromMemory(data, width, height, true /* srgb */);
    free(data);

    if (texId) {
        panelObject->SetSurfaceTexture(
            0, 0, SURFACE_TEXTURE_DIFFUSE, texId, ThumbWidth, ThumbHeight);

        panel->TextureId = texId;

        BuildTextureMipmaps(texId);
        MakeTextureTrilinear(texId);
        MakeTextureClamped(texId);
    }
}

void OvrFolderBrowser::LoadFolderViewPanels(
    OvrGuiSys& guiSys,
    const OvrMetaData& metaData,
    const OvrMetaData::Category& category,
    const int folderIndex,
    FolderView& folder,
    std::vector<VRMenuObjectParms const*>& outParms) {
    // Build panels
    std::vector<const OvrMetaDatum*> categoryPanos;
    metaData.GetMetaData(category, categoryPanos);
    const int numPanos = static_cast<int>(categoryPanos.size());
    ALOG("Building %d panels for %s", numPanos, category.CategoryTag.c_str());
    for (int panoIndex = 0; panoIndex < numPanos; panoIndex++) {
        AddPanelToFolder(
            guiSys,
            const_cast<OvrMetaDatum* const>(categoryPanos.at(panoIndex)),
            folderIndex,
            folder,
            outParms);
    }
}

void OvrFolderBrowser::AddPanelToFolder(
    OvrGuiSys& guiSys,
    const OvrMetaDatum* panoData,
    const int folderIndex,
    FolderView& folder,
    std::vector<VRMenuObjectParms const*>& outParms) {
    assert(panoData);

    const int panelIndex = static_cast<int>(folder.Panels.size());
    PanelView* panel = CreatePanelView(panelIndex);
    panel->MenuId = NextId();

    // This is the only place these indices are ever set.
    panoData->FolderIndex = folderIndex;
    panoData->PanelId = panelIndex;

    // Panel placement - based on index which determines position within the circumference
    const float factor = (float)panelIndex / (float)CircumferencePanelSlots;
    Quatf rot(DOWN, (MATH_FLOAT_TWOPI * factor));
    Vector3f dir(FWD * rot);
    Posef panelPose(rot, dir * Radius);
    Vector3f panelScale(1.0f);

    VRMenuObject* swipeObject = guiSys.GetVRMenuMgr().ToObject(folder.SwipeHandle);
    assert(swipeObject != NULL);

    // Add panel VRMenuObjectParms to be added to our menu
    AddPanelMenuObject(
        guiSys,
        panoData,
        swipeObject,
        panel->MenuId,
        &folder,
        folderIndex,
        panel,
        panelPose,
        panelScale,
        outParms);

    folder.Panels.push_back(panel);
}

void OvrFolderBrowser::QueueAsyncThumbnailLoad(
    const OvrMetaDatum* panoData,
    const int folderIndex,
    const int panelId) {
    // Verify input
    if (panoData == NULL) {
        ALOGW("OvrFolderBrowser::QueueAsyncThumbnailLoad error - NULL panoData");
        return;
    }

    if (folderIndex < 0 || folderIndex >= static_cast<int>(Folders.size())) {
        ALOGW(
            "OvrFolderBrowser::QueueAsyncThumbnailLoad error invalid folder index: %d",
            folderIndex);
        return;
    }

    FolderView* folder = GetFolderView(folderIndex);
    if (folder == NULL) {
        ALOGW(
            "OvrFolderBrowser::QueueAsyncThumbnailLoad error invalid folder index: %d",
            folderIndex);
        return;
    } else {
        const int numPanels = static_cast<int>(folder->Panels.size());
        if (panelId < 0 || panelId >= numPanels) {
            ALOGW("OvrFolderBrowser::QueueAsyncThumbnailLoad error invalid panel id: %d", panelId);
            return;
        }
    }

    // Create or load thumbnail - request built up here to be processed ThumbnailThread
    const std::string panoUrl = ThumbUrl(panoData);
    const std::string thumbName = ThumbName(panoUrl);
    std::string finalThumb;
    char relativeThumbPath[1024];
    ToRelativePath(ThumbSearchPaths, panoUrl.c_str(), relativeThumbPath, sizeof(relativeThumbPath));

    char appCacheThumbPath[1024];
    OVR::OVR_sprintf(
        appCacheThumbPath,
        sizeof(appCacheThumbPath),
        "%s%s",
        AppCachePath.c_str(),
        ThumbName(relativeThumbPath).c_str());

    // if this url doesn't exist locally
    if (!FileExists(panoUrl.c_str())) {
        // Check app cache to see if we already downloaded it
        if (FileExists(appCacheThumbPath)) {
            finalThumb = appCacheThumbPath;
        } else // download and cache it
        {
            BackgroundCommands.PostPrintf(
                "httpThumb %s %s %d %d", panoUrl.c_str(), appCacheThumbPath, folderIndex, panelId);
            return;
        }
    }

    if (finalThumb.empty()) {
        // Try search paths next to image for retail photos
        if (!GetFullPath(ThumbSearchPaths, thumbName.c_str(), finalThumb)) {
            // Try app cache for cached user pano thumbs
            if (FileExists(appCacheThumbPath)) {
                finalThumb = appCacheThumbPath;
            } else {
                const std::string altThumbPath = AlternateThumbName(panoUrl);
                if (altThumbPath.empty() ||
                    !GetFullPath(ThumbSearchPaths, altThumbPath.c_str(), finalThumb)) {
                    int pathLen = static_cast<int>(panoUrl.length());
                    if (pathLen > 2 && OVR::OVR_stricmp(panoUrl.c_str() + pathLen - 2, ".x") == 0) {
                        ALOGW("Thumbnails cannot be generated from encrypted images.");
                        return; // No thumb & can't create
                    }
                }
            }
        }
    }

    if (!finalThumb.empty()) {
        char cmd[1024];
        OVR::OVR_sprintf(cmd, 1024, "load %i %i:%s", folderIndex, panelId, finalThumb.c_str());
        ALOG("Thumb cmd: %s", cmd);
        BackgroundCommands.PostString(cmd);
    } else {
        ALOGW("Failed to find thumbnail for %s - will be created when selected", panoUrl.c_str());
    }
}

void OvrFolderBrowser::AddPanelMenuObject(
    OvrGuiSys& guiSys,
    const OvrMetaDatum* panoData,
    VRMenuObject* parent,
    VRMenuId_t id,
    FolderView* folder,
    const int folderIndex,
    PanelView* panel,
    Posef panelPose,
    Vector3f panelScale,
    std::vector<VRMenuObjectParms const*>& outParms) {
    // Load a panel
    std::vector<VRMenuComponent*> panelComps;
    panelComps.push_back(new OvrPanel_OnUp(this, panoData, folderIndex, panel->Id));
    panelComps.push_back(
        new OvrDefaultComponent(Vector3f(0.0f, 0.0f, 0.05f), 1.05f, 0.25f, 0.25f, Vector4f(0.f)));

    // single-pass multitexture
    VRMenuSurfaceParms panelSurfParms(
        "panelSurface",
        DefaultPanelTextureIds[0],
        ThumbWidth,
        ThumbHeight,
        SURFACE_TEXTURE_DIFFUSE,
        DefaultPanelTextureIds[1],
        ThumbWidth,
        ThumbHeight,
        SURFACE_TEXTURE_DIFFUSE,
        0,
        0,
        0,
        SURFACE_TEXTURE_MAX);

    // Title text placed below thumbnail
    const Posef textPose(Quatf(), Vector3f(0.0f, -PanelHeight * PanelTextSpacingScale, 0.0f));

    std::string panelTitle = GetPanelTitle(guiSys, *panoData);
    // GetPanelTitle should ALWAYS return a localized string
    // guiSys.GetApp()->GetLocale()->GetString( panoTitle, panoTitle, panelTitle );

    const VRMenuFontParms fontParms(
        HORIZONTAL_CENTER, VERTICAL_CENTER, false, false, true, 0.525f, 0.45f, 0.5f);
    VRMenuObjectParms* p = new VRMenuObjectParms(
        VRMENU_BUTTON,
        panelComps,
        panelSurfParms,
        panelTitle.c_str(),
        panelPose,
        panelScale,
        textPose,
        Vector3f(1.0f),
        fontParms,
        id,
        VRMenuObjectFlags_t(VRMENUOBJECT_DONT_HIT_TEXT) | VRMENUOBJECT_RENDER_HIERARCHY_ORDER,
        VRMenuObjectInitFlags_t(VRMENUOBJECT_INIT_FORCE_POSITION));
    outParms.push_back(p);
}

bool OvrFolderBrowser::ApplyThumbAntialiasing(unsigned char* inOutBuffer, int width, int height)
    const {
    if (inOutBuffer != NULL) {
        if (ThumbPanelBG != NULL) {
            const unsigned thumbWidth = GetThumbWidth();
            const unsigned thumbHeight = GetThumbHeight();

            const int numBytes = width * height * 4;
            const int thumbPanelBytes = thumbWidth * thumbHeight * 4;
            if (numBytes != thumbPanelBytes) {
                ALOGW("OvrFolderBrowser::ApplyAA - Thumbnail image is the wrong size!");
            } else {
                // Apply alpha from vrappframework/res/raw to alpha channel for anti-aliasing
                for (int i = 3; i < thumbPanelBytes; i += 4) {
                    inOutBuffer[i] = ThumbPanelBG[i];
                }
                return true;
            }
        }
    }
    return false;
}

const OvrFolderBrowser::FolderView* OvrFolderBrowser::GetFolderView(int index) const {
    if (Folders.empty()) {
        return NULL;
    }

    if (index < 0 || index >= static_cast<int>(Folders.size())) {
        return NULL;
    }

    return Folders.at(index);
}

OvrFolderBrowser::FolderView* OvrFolderBrowser::GetFolderView(int index) {
    if (Folders.empty()) {
        return NULL;
    }

    if (index < 0 || index >= static_cast<int>(Folders.size())) {
        return NULL;
    }

    return Folders.at(index);
}

OvrFolderBrowser::FolderView* OvrFolderBrowser::GetFolderView(const std::string& categoryTag) {
    if (Folders.empty()) {
        return NULL;
    }

    for (FolderView* currentFolder : Folders) {
        if (currentFolder->CategoryTag == categoryTag) {
            return currentFolder;
        }
    }

    return NULL;
}

bool OvrFolderBrowser::RotateCategory(OvrGuiSys& guiSys, const CategoryDirection direction) {
    OvrFolderSwipeComponent* swipeComp =
        const_cast<OvrFolderSwipeComponent*>(GetSwipeComponentForActiveFolder(guiSys));
    return swipeComp->Rotate(direction);
}

void OvrFolderBrowser::SetCategoryRotation(
    OvrGuiSys& guiSys,
    const int folderIndex,
    const int panelIndex) {
    assert(folderIndex >= 0 && folderIndex < static_cast<int>(Folders.size()));
    const FolderView* folder = GetFolderView(folderIndex);
    if (folder != NULL) {
        VRMenuObject* swipe = guiSys.GetVRMenuMgr().ToObject(folder->SwipeHandle);
        assert(swipe);

        OvrFolderSwipeComponent* swipeComp = swipe->GetComponentById<OvrFolderSwipeComponent>();
        assert(swipeComp);

        swipeComp->SetRotationByIndex(panelIndex);
    }
}

void OvrFolderBrowser::OnPanelUp(OvrGuiSys& guiSys, const OvrMetaDatum* data) {
    if (AllowPanelTouchUp) {
        OnPanelActivated(guiSys, data);
    }
}

const OvrMetaDatum* OvrFolderBrowser::NextFileInDirectory(OvrGuiSys& guiSys, const int step) {
    const OvrMetaDatum* datum = GetNextFileInCategory(guiSys, step);
    if (datum) {
        const FolderView* folder = GetFolderView(GetActiveFolderIndex(guiSys));
        if (folder == NULL) {
            return NULL;
        }

        const VRMenuObject* swipeObject = guiSys.GetVRMenuMgr().ToObject(folder->SwipeHandle);
        if (swipeObject) {
            OvrFolderSwipeComponent* swipeComp =
                swipeObject->GetComponentById<OvrFolderSwipeComponent>();
            if (swipeComp) {
                const int nextPanelIndex = swipeComp->GetNextPanelIndex(step);
                swipeComp->SetRotationByIndex(nextPanelIndex);
                return datum;
            }
        }
    }

    return NULL;
}

const OvrMetaDatum* OvrFolderBrowser::GetNextFileInCategory(OvrGuiSys& guiSys, const int step)
    const {
    const FolderView* folder = GetFolderView(GetActiveFolderIndex(guiSys));
    if (folder == NULL) {
        return NULL;
    }

    const int numPanels = static_cast<int>(folder->Panels.size());

    if (numPanels == 0) {
        return NULL;
    }

    const OvrFolderSwipeComponent* swipeComp = GetSwipeComponentForActiveFolder(guiSys);
    if (swipeComp) {
        const int nextPanelIndex = swipeComp->GetNextPanelIndex(step);

        const PanelView* panel = folder->Panels.at(nextPanelIndex);
        VRMenuObject* panelObject = guiSys.GetVRMenuMgr().ToObject(panel->Handle);
        if (panelObject) {
            const OvrPanel_OnUp* panelUpComp = panelObject->GetComponentById<OvrPanel_OnUp>();
            if (panelUpComp) {
                return panelUpComp->GetData();
            }
        }
    }

    return NULL;
}

float OvrFolderBrowser::CalcFolderMaxRotation(const FolderView* folder) const {
    if (folder == NULL) {
        assert(false);
        return 0.0f;
    }
    int numPanels = clamp<int>(static_cast<int>(folder->Panels.size()) - 1, 0, INT_MAX);
    return static_cast<float>(numPanels);
}

const OvrFolderSwipeComponent* OvrFolderBrowser::GetSwipeComponentForActiveFolder(
    OvrGuiSys& guiSys) const {
    const FolderView* folder = GetFolderView(GetActiveFolderIndex(guiSys));
    if (folder == NULL) {
        assert(false);
        return NULL;
    }

    const VRMenuObject* swipeObject = guiSys.GetVRMenuMgr().ToObject(folder->SwipeHandle);
    if (swipeObject) {
        return swipeObject->GetComponentById<OvrFolderSwipeComponent>();
    }

    return NULL;
}

bool OvrFolderBrowser::GazingAtMenu(Matrix4f const& view) const {
    if (GetFocusedHandle().IsValid()) {
        Vector3f viewForwardFlat(view.M[2][0], 0.0f, view.M[2][2]);
        viewForwardFlat.Normalize();

        const float cosHalf = cosf(VisiblePanelsArcAngle);
        const float dot = viewForwardFlat.Dot(-FWD * GetMenuPose().Rotation);

        if (dot >= cosHalf) {
            return true;
        }
    }
    return false;
}

int OvrFolderBrowser::GetActiveFolderIndex(OvrGuiSys& guiSys) const {
    VRMenuObject* rootObject = guiSys.GetVRMenuMgr().ToObject(GetRootHandle());
    assert(rootObject);

    OvrFolderBrowserRootComponent* rootComp =
        rootObject->GetComponentById<OvrFolderBrowserRootComponent>();
    assert(rootComp);

    return rootComp->GetCurrentIndex(guiSys, rootObject);
}

void OvrFolderBrowser::SetActiveFolder(OvrGuiSys& guiSys, int folderIdx) {
    VRMenuObject* rootObject = guiSys.GetVRMenuMgr().ToObject(GetRootHandle());
    assert(rootObject);

    OvrFolderBrowserRootComponent* rootComp =
        rootObject->GetComponentById<OvrFolderBrowserRootComponent>();
    assert(rootComp);

    rootComp->SetActiveFolder(folderIdx);
}

void OvrFolderBrowser::TouchDown() {
    IsTouchDownPosistionTracked = false;
    TouchDirectionLocked = NO_LOCK;
}

void OvrFolderBrowser::TouchUp() {
    IsTouchDownPosistionTracked = false;
    TouchDirectionLocked = NO_LOCK;
}

void OvrFolderBrowser::TouchRelative(Vector3f touchPos) {
    if (!IsTouchDownPosistionTracked) {
        IsTouchDownPosistionTracked = true;
        TouchDownPosistion = touchPos;
    }

    if (TouchDirectionLocked == NO_LOCK) {
        float xAbsChange = fabsf(TouchDownPosistion.x - touchPos.x);
        float yAbsChange = fabsf(TouchDownPosistion.y - touchPos.y);

        if (xAbsChange >= SCROLL_DIRECTION_DECIDING_DISTANCE ||
            yAbsChange >= SCROLL_DIRECTION_DECIDING_DISTANCE) {
            if (xAbsChange >= yAbsChange) {
                TouchDirectionLocked = HORIZONTAL_LOCK;
            } else {
                TouchDirectionLocked = VERTICAL_LOCK;
            }
        }
    }
}

OvrFolderBrowser::FolderView::FolderView(const std::string& name, const std::string& tag)
    : CategoryTag(tag), LocalizedName(name), FolderIndex(-1), MaxRotation(0.0f), Visible(false) {}

OvrFolderBrowser::FolderView::~FolderView() {
    // Thumbnails deallocated in FolderBrowser destructor
    DeletePointerArray(Panels);
}

void OvrFolderBrowser::FolderView::UnloadThumbnails(
    OvrGuiSys& guiSys,
    const GLuint defaultTextureId,
    const int thumbWidth,
    const int thumbHeight) {
    FreeThumbnailTextures(defaultTextureId);

    for (PanelView* panel : Panels) {
        if (panel) {
            panel->LoadDefaultThumbnail(guiSys, defaultTextureId, thumbWidth, thumbHeight);
        }
    }
}

void OvrFolderBrowser::FolderView::FreeThumbnailTextures(const GLuint defaultTextureId) {
    for (PanelView* panel : Panels) {
        if (panel && (panel->TextureId != defaultTextureId)) {
            glDeleteTextures(1, &panel->TextureId);
            panel->TextureId = 0;
        }
    }
}

void OvrFolderBrowser::PanelView::LoadDefaultThumbnail(
    OvrGuiSys& guiSys,
    const GLuint defaultTextureId,
    const int thumbWidth,
    const int thumbHeight) {
    VRMenuObject* panelObject = guiSys.GetVRMenuMgr().ToObject(Handle);

    if (panelObject) {
        ALOG("Setting thumb to default texture %d", defaultTextureId);
        panelObject->SetSurfaceTexture(
            0, 0, SURFACE_TEXTURE_DIFFUSE, defaultTextureId, thumbWidth, thumbHeight);
    }

    Visible = false;
}

} // namespace OVRFW
