#pragma once
#include "../MenuPage.h"

#include <memory>
#include <vector>

class MenuList;
struct AppSettings;

// Per-button emulator input mapping. One row per Virtual Boy button;
// pressing a row starts a "press any button now" capture (see Menu::
// CaptureHook) that binds it in AppSettings::vbButtons - consumed by
// ButtonMapper::TranslateToVBBitmask on both platforms (see pc2d's
// PollGameplayInput / OpenXrApp's gameplay-bit construction).
class EmulatorButtonMapPage : public MenuPage
{
public:
    MenuPage *settingsPage = nullptr;

    void Init(UiRenderer &ui, const UiMenuResources &resources) override;

private:
    void StartCapture(int rowIndex, uint32_t vbBit);
    void ResetMapping();
    void RefreshLabels();

    std::vector<std::shared_ptr<MenuList::Entry>> m_rowEntries;
    AppSettings *m_settings = nullptr;
};
