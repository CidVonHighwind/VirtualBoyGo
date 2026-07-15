#pragma once
#include "../MenuPage.h"

#include <memory>
#include <vector>

class MenuList;
struct AppSettings;

// Per-button controller input mapping, laid out like the reference: one row
// per Virtual Boy button, showing its icon and two side-by-side binding
// columns (primary + secondary). Left/Right move the highlight between the
// two columns; pressing a column starts a "press any button now" capture
// (see Menu::CaptureHook) that binds that slot in AppSettings::vbButtons -
// consumed by ButtonMapper::TranslateToVBBitmask (either binding triggers
// the VB button).
class EmulatorButtonMapPage : public MenuPage
{
public:
    MenuPage *settingsPage = nullptr;

    void Init(UiRenderer &ui, const UiMenuResources &resources) override;

private:
    // buttonIndex: row into kButtons; column: 0 = primary, 1 = secondary.
    void StartCapture(int buttonIndex, int column);
    void ResetMapping();
    void ApplyDefaultMapping(bool overwrite);
    void RefreshLabels();

    std::shared_ptr<MenuList> m_list; // kept for GetActiveColumn() at press time
    std::vector<std::shared_ptr<MenuList::Entry>> m_rowEntries; // one per button
    AppSettings *m_settings = nullptr;
    ButtonMappingProfile m_mappingProfile = ButtonMappingProfile::Vr;
};
