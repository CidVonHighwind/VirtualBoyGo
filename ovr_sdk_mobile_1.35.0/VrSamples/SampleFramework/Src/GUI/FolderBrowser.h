/************************************************************************************

Filename    :   FolderBrowser.h
Content     :   A menu for browsing a hierarchy of folders with items represented by thumbnails.
Created     :   July 25, 2014
Authors     :   Jonathan E. Wright, Warsam Osman, Madhu Kalva

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#pragma once

#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "VRMenu.h"
#include "MessageQueue.h"
#include "MetaDataManager.h"
#include "ScrollManager.h"
#include "VRMenuComponent.h"

namespace OVRFW {

class OvrFolderBrowserRootComponent;
class OvrFolderSwipeComponent;
class OvrFolderBrowserSwipeComponent;
class OvrDefaultComponent;
class OvrPanel_OnUp;

//==============================================================
// OvrFolderBrowser
class OvrFolderBrowser : public VRMenu {
   public:
    struct PanelView {
        PanelView() : Id(-1), TextureId(0), Visible(false), MenuId(0) {}

        PanelView(int id) : Id(id), TextureId(0), Visible(false), MenuId(0) {}

        PanelView(int id, GLuint textId) : Id(id), TextureId(textId), Visible(false), MenuId(0) {}

        // private assignment operator to prevent copying
        PanelView& operator=(PanelView&);

        virtual ~PanelView() {}

        virtual menuHandle_t GetThumbnailHandle() {
            return Handle;
        }

        void LoadDefaultThumbnail(
            OvrGuiSys& guiSys,
            const GLuint defaultTextureId,
            const int thumbWidth,
            const int thumbHeight);

        const int Id; // Unique id for thumbnail loading
        menuHandle_t Handle; // Handle to the panel
        GLuint TextureId; // Texture id - PanelView maintains ownership
        volatile bool Visible; // Set in main thread and queried in thumbnail thread

        VRMenuId_t MenuId;
    };

    struct FolderView {
        FolderView(const std::string& name, const std::string& tag);

        // private assignment operator to prevent copying
        FolderView& operator=(FolderView&);

        virtual ~FolderView();

        void UnloadThumbnails(
            OvrGuiSys& guiSys,
            const GLuint defaultTextureId,
            const int thumbWidth,
            const int thumbHeight);
        void FreeThumbnailTextures(const GLuint defaultTextureId);

        const std::string CategoryTag;
        const std::string LocalizedName; // Store for rebuild of title
        int FolderIndex;
        menuHandle_t Handle; // Handle to main root - parent to both Title and Panels
        menuHandle_t TitleRootHandle; // Handle to the folder title root
        menuHandle_t TitleHandle; // Handle to the folder title
        menuHandle_t SwipeHandle; // Handle to root for panels
        menuHandle_t ScrollBarHandle; // Handle to the scrollbar object
        float MaxRotation; // Used by SwipeComponent
        volatile bool Visible; // Set in main thread and queried in thumbnail thread
        std::vector<PanelView*> Panels;
    };

    static char const* MENU_NAME;
    static VRMenuId_t ID_CENTER_ROOT;

    virtual void OneTimeInit(OvrGuiSys& guiSys);

    // Builds the menu view using the passed in model. Builds only what's marked dirty - can be
    // called multiple times.
    virtual void BuildDirtyMenu(OvrGuiSys& guiSys, OvrMetaData& metaData);

    // Swiping when the menu is inactive can cycle through files in
    // the directory.  Step can be 1 or -1.
    virtual const OvrMetaDatum* NextFileInDirectory(OvrGuiSys& guiSys, const int step);
    virtual const OvrMetaDatum* GetNextFileInCategory(OvrGuiSys& guiSys, const int step) const;

    // Called if touch up is activated without a focused item
    // User returns true if consumed
    virtual bool OnTouchUpNoFocused(OvrGuiSys& /*guiSys*/) {
        return false;
    }

    FolderView* GetFolderView(const std::string& categoryTag);
    FolderView* GetFolderView(int index);
    ovrMessageQueue& GetTextureCommands() {
        return TextureCommands;
    }
    void SetPanelTextSpacingScale(const float scale) {
        PanelTextSpacingScale = scale;
    }
    void SetFolderTitleSpacingScale(const float scale) {
        FolderTitleSpacingScale = scale;
    }
    void SetScrollBarSpacingScale(const float scale) {
        ScrollBarSpacingScale = scale;
    }
    void SetScrollBarRadiusScale(const float scale) {
        ScrollBarRadiusScale = scale;
    }
    void SetAllowPanelTouchUp(const bool allow) {
        AllowPanelTouchUp = allow;
    }

    enum RootDirection { MOVE_ROOT_NONE, MOVE_ROOT_UP, MOVE_ROOT_DOWN };
    // Attempts to scroll - returns false if not possible due to being at boundary or currently
    // scrolling
    bool ScrollRoot(const RootDirection direction, bool hideScrollHint = false);

    enum CategoryDirection { MOVE_PANELS_NONE, MOVE_PANELS_LEFT, MOVE_PANELS_RIGHT };
    bool RotateCategory(OvrGuiSys& guiSys, const CategoryDirection direction);
    void SetCategoryRotation(OvrGuiSys& guiSys, const int folderIndex, const int panelIndex);

    void TouchDown();
    void TouchUp();
    void TouchRelative(OVR::Vector3f touchPos);

    // Accessors
    const FolderView* GetFolderView(int index) const;
    int GetNumFolders() const {
        return static_cast<int>(Folders.size());
    }
    int GetCircumferencePanelSlots() const {
        return CircumferencePanelSlots;
    }
    float GetRadius() const {
        return Radius;
    }
    float GetPanelHeight() const {
        return PanelHeight;
    }
    float GetPanelWidth() const {
        return PanelWidth;
    }
    // The index for the folder that's at the center - considered the actively selected folder
    int GetActiveFolderIndex(OvrGuiSys& guiSys) const;
    // Returns the number of panels shown per folder - or Swipe Component
    int GetNumSwipePanels() const {
        return NumSwipePanels;
    }
    unsigned GetThumbWidth() const {
        return ThumbWidth;
    }
    unsigned GetThumbHeight() const {
        return ThumbHeight;
    }
    bool HasNoMedia() const {
        return NoMedia;
    }
    bool GazingAtMenu(OVR::Matrix4f const& view) const;

    const OvrFolderSwipeComponent* GetSwipeComponentForActiveFolder(OvrGuiSys& guiSys) const;

    void SetScrollHintVisible(const bool visible);
    void SetActiveFolder(OvrGuiSys& guiSys, int folderIdx);

    eScrollDirectionLockType GetControllerDirectionLock() {
        return ControllerDirectionLock;
    }
    eScrollDirectionLockType GetTouchDirectionLock() {
        return TouchDirectionLocked;
    }
    bool ApplyThumbAntialiasing(unsigned char* inOutBuffer, int width, int height) const;
    GLuint GetDefaultThumbnailTextureId() const {
        return DefaultPanelTextureIds[0];
    }
    void
    QueueAsyncThumbnailLoad(const OvrMetaDatum* panoData, const int folderIndex, const int panelId);

   protected:
    OvrFolderBrowser(
        OvrGuiSys& guiSys,
        OvrMetaData& metaData,
        float panelWidth,
        float panelHeight,
        float radius,
        unsigned numSwipePanels,
        unsigned thumbWidth,
        unsigned thumbHeight);

    virtual ~OvrFolderBrowser();

    //================================================================================
    // Subclass protected interface

    // Called from the base class when building a cateory.
    virtual std::string GetCategoryTitle(OvrGuiSys& guiSys, const char* tag, const char* key)
        const = 0;

    // Called from the base class when building a panel
    virtual std::string GetPanelTitle(OvrGuiSys& guiSys, const OvrMetaDatum& panelData) const = 0;

    // Called when a panel is activated
    virtual void OnPanelActivated(OvrGuiSys& guiSys, const OvrMetaDatum* panelData) = 0;

    // Called on a background thread to load thumbnail
    virtual unsigned char* LoadThumbnail(const char* filename, int& width, int& height) = 0;

    // Returns the proper thumbnail URL
    virtual std::string ThumbUrl(const OvrMetaDatum* item) {
        return item->Url;
    }

    // Adds thumbnail extension to a file to find/create its thumbnail
    virtual std::string ThumbName(const std::string& s) = 0;

    // Media not found - have subclass set the title, image and caption to display
    virtual void OnMediaNotFound(
        OvrGuiSys& guiSys,
        std::string& title,
        std::string& imageFile,
        std::string& message) = 0;

    // Optional interface
    //
    // Request external thumbnail - called on main thread
    virtual unsigned char* RetrieveRemoteThumbnail(
        const char* /*url*/,
        const char* /*cacheDestinationFile*/,
        int /*folderId*/,
        int /*panelId*/,
        int& /*outWidth*/,
        int& /*outHeight*/) {
        return NULL;
    }

    // If we fail to load one type of thumbnail, try an alternative
    virtual std::string AlternateThumbName(const std::string& /*thumbNameAlt*/) {
        return std::string();
    }

    // Called on opening menu
    virtual void OnBrowserOpen(OvrGuiSys& /*guiSys*/) {}

    //================================================================================

    // OnEnterMenuRootAdjust is set to be performed the
    // next time the menu is opened to adjust for a potentially deleted or added category
    void SetRootAdjust(const RootDirection dir) {
        OnEnterMenuRootAdjust = dir;
    }
    RootDirection GetRootAdjust() const {
        return OnEnterMenuRootAdjust;
    }

    // Rebuilds a folder using passed in data
    void RebuildFolderView(
        OvrGuiSys& guiSys,
        OvrMetaData& metaData,
        const int folderIndex,
        const std::vector<const OvrMetaDatum*>& data);

    virtual void AddPanelMenuObject(
        OvrGuiSys& guiSys,
        const OvrMetaDatum* panoData,
        VRMenuObject* parent,
        VRMenuId_t id,
        FolderView* folder,
        int const folderIndex,
        PanelView* panel,
        OVR::Posef panelPose,
        OVR::Vector3f panelScale,
        std::vector<VRMenuObjectParms const*>& outParms);

    virtual FolderView* CreateFolderView(
        std::string localizedCategoryName,
        std::string categoryTag) {
        return new FolderView(localizedCategoryName, categoryTag);
    }

    virtual PanelView* CreatePanelView(int panelIndex) {
        return new PanelView(panelIndex);
    }

    virtual VRMenuComponent* CreateFolderBrowserSwipeComponent(
        OvrFolderBrowser& folderBrowser,
        OvrFolderBrowser::FolderView* folder);

    virtual void Frame_Impl(OvrGuiSys& guiSys, ovrApplFrameIn const& vrFrame);

    VRMenuId_t NextId();

   protected:
    int MediaCount; // Used to determine if no media was loaded

   private:
    void ThumbnailThread();
    void LoadThumbnailToTexture(OvrGuiSys& guiSys, const char* thumbnailCommand);

    friend class OvrPanel_OnUp;
    void OnPanelUp(OvrGuiSys& guiSys, const OvrMetaDatum* data);

    virtual void Open_Impl(OvrGuiSys& guiSys);
    virtual void Close_Impl(OvrGuiSys& guiSys);

    void BuildFolderView(
        OvrGuiSys& guiSys,
        OvrMetaData::Category& category,
        FolderView* const folder,
        const OvrMetaData& metaData,
        VRMenuId_t foldersRootId,
        const int folderIndex);

    void LoadFolderViewPanels(
        OvrGuiSys& guiSys,
        const OvrMetaData& metaData,
        const OvrMetaData::Category& category,
        const int folderIndex,
        FolderView& folder,
        std::vector<VRMenuObjectParms const*>& outParms);

    void AddPanelToFolder(
        OvrGuiSys& guiSys,
        const OvrMetaDatum* panoData,
        const int folderIndex,
        FolderView& folder,
        std::vector<VRMenuObjectParms const*>& outParms);

    void DisplaceFolder(
        int index,
        const OVR::Vector3f& direction,
        float distance,
        bool startOffSelf = false);
    void UpdateFolderTitle(OvrGuiSys& guiSys, const FolderView* folder);
    float CalcFolderMaxRotation(const FolderView* folder) const;

    // Members
    VRMenuId_t FoldersRootId;
    float PanelWidth;
    float PanelHeight;
    int ThumbWidth;
    int ThumbHeight;
    float PanelTextSpacingScale; // Panel text placed with vertical position of -panelHeight *
                                 // PanelTextSpacingScale
    float FolderTitleSpacingScale; // Folder title placed with vertical position of PanelHeight *
                                   // FolderTitleSpacingScale
    float ScrollBarSpacingScale; // Scroll bar placed with vertical position of PanelHeight *
                                 // ScrollBarSpacingScale
    float ScrollBarRadiusScale; // Scroll bar placed with horizontal position of FWD * Radius *
                                // ScrollBarRadiusScale

    int CircumferencePanelSlots;
    unsigned NumSwipePanels;
    float Radius;
    float VisiblePanelsArcAngle;
    bool SwipeHeldDown;
    float DebounceTime;
    bool NoMedia;
    bool AllowPanelTouchUp;

    std::vector<FolderView*> Folders;

    menuHandle_t ScrollSuggestionRootHandle;

    RootDirection OnEnterMenuRootAdjust;

    // Checked at Frame() time for commands from the thumbnail/create thread
    ovrMessageQueue TextureCommands;
    ovrMessageQueue BackgroundCommands;

    enum eThumbnailThreadState {
        THUMBNAIL_THREAD_WORK,
        THUMBNAIL_THREAD_PAUSE,
        THUMBNAIL_THREAD_SHUTDOWN
    };
    std::atomic<eThumbnailThreadState> ThumbnailThreadState;
    std::mutex ThumbnailThreadMutex;
    std::condition_variable ThumbnailThreadCondition;

    std::vector<std::string> ThumbSearchPaths;
    std::string AppCachePath;

    // Keep a reference to Panel texture used for AA alpha when creating thumbnails
    static unsigned char* ThumbPanelBG;

    // Default panel textures (base and highlight) - loaded once
    GlTexture DefaultPanelTextureIds[2];

    // Restricted Scrolling
    static const float
        CONTROLER_COOL_DOWN; // Controller goes to rest very frequently so cool down helps
    eScrollDirectionLockType ControllerDirectionLock;
    float LastControllerInputTimeStamp;

    static const float SCROLL_DIRECTION_DECIDING_DISTANCE;
    bool IsTouchDownPosistionTracked;
    OVR::Vector3f
        TouchDownPosistion; // First event in touch relative is considered as touch down position
    eScrollDirectionLockType TouchDirectionLocked;

    std::thread ThumbnailLoadingThread;
};

//==============================================================
// OvrPanel_OnUp
// Forwards a touch up from Panel to Menu
// Holds a pointer to the datum panel represents
class OvrPanel_OnUp : public VRMenuComponent_OnTouchUp {
   public:
    static const int TYPE_ID = 1107;

    OvrPanel_OnUp(
        VRMenu* menu,
        const OvrMetaDatum* panoData,
        int const folderIndex,
        int const panelId)
        : VRMenuComponent_OnTouchUp(),
          Menu(menu),
          Data(panoData),
          FolderIndex(folderIndex),
          PanelId(panelId) {
        OVR_ASSERT(Data);
    }

    virtual ~OvrPanel_OnUp() {
        Menu = NULL;
        Data = NULL;
        FolderIndex = -1;
        PanelId = -1;
    }

    void SetData(const OvrMetaDatum* panoData) {
        Data = panoData;
    }

    virtual int GetTypeId() const {
        return TYPE_ID;
    }
    const OvrMetaDatum* GetData() const {
        return Data;
    }

   protected:
    virtual eMsgStatus OnEvent_Impl(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event) {
        OVR_UNUSED(vrFrame);
        OVR_UNUSED(self);
        OVR_UNUSED(event);

        OVR_ASSERT(event.EventType == VRMENU_EVENT_TOUCH_UP);
        if (OvrFolderBrowser* folderBrowser = static_cast<OvrFolderBrowser*>(Menu)) {
            folderBrowser->OnPanelUp(guiSys, Data);
        }

        return MSG_STATUS_CONSUMED;
    }

   private:
    VRMenu* Menu; // menu that holds the button
    const OvrMetaDatum* Data; // Payload for this button
    int FolderIndex; // index of the folder
    int PanelId; // id of panel
};

} // namespace OVRFW
