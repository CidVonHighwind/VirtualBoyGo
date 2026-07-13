#pragma once

#include "MenuWidgets.h"
#include "UiMenuResources.h"
#include "UiRenderer.h"

#include <functional>

// Base class for a single menu screen. Each subclass owns a Menu (the
// widget container / navigation state machine) and a Navigate callback
// that AppMenu wires up at init time so pages can push/pop transitions
// without knowing about each other.
//
// Usage:
//   page.Navigate = [&](MenuPage* target, int dir) { StartTransition(target, dir); };
//   page.Init(ui, resources);
//   page.Update(btn, lastBtn, dt);
//   page.Draw(ui, transitionDirX, moveProgress, moveDist, fadeProgress);
class MenuPage
{
public:
    virtual ~MenuPage() = default;

    // Called once after resources are loaded and Navigate is set.
    virtual void Init(UiRenderer &ui, const UiMenuResources &resources) = 0;

    // Navigate to another page. Set by AppMenu before Init().
    // dir: 1 = slide next page in from right, -1 = from left.
    std::function<void(MenuPage *target, int dir)> Navigate;

    void Update(uint32_t *buttonState, uint32_t *lastButtonState, float deltaSeconds)
    {
        m_menu.Update(buttonState, lastButtonState, deltaSeconds);
    }

    void Draw(UiRenderer &ui, int transitionDirX, float moveProgress, float moveDist, float fadeProgress)
    {
        m_menu.Draw(ui, transitionDirX, 0, moveProgress, moveDist, fadeProgress);
    }

    // Puts the cursor back on the first entry - called by AppMenu whenever
    // this page becomes current, so re-entering a page never leaves the
    // cursor wherever it was last left (e.g. MainPage should always land on
    // "Resume", not wherever the user was before loading a ROM). Virtual so
    // MainPage can piggyback a save-preview refresh onto it - this is the
    // only hook AppMenu calls when a ROM load jumps straight back to
    // MainPage (see AppMenu::StartTransition's closed-menu instant-jump
    // path), so it's the only place a freshly-loaded ROM's existing saves
    // can get picked up without waiting for the user to touch Save/the slot
    // stepper first.
    virtual void ResetSelection() { m_menu.ResetSelection(); }

    // Applies the persisted "swap select/back" setting to this page's own
    // Menu - called by AppMenu on every page right after settings load (and
    // again whenever MenuButtonMapPage's toggle changes it), since each page
    // owns its own Menu instance rather than sharing one.
    void SetSwapSelectBackButton(bool swap) { m_menu.SwapSelectBackButton = swap; }

    // Applies the persisted extra select-button bindings (see Menu::
    // ExtraSelectButton1/2's doc comment) to this page's own Menu.
    void SetExtraSelectButtons(const ButtonMapper::MappedButton &b1, const ButtonMapper::MappedButton &b2)
    {
        m_menu.ExtraSelectButton1 = b1;
        m_menu.ExtraSelectButton2 = b2;
    }

    // Lets a page temporarily suspend this Menu's normal navigation dispatch
    // to capture the next raw button press instead - see Menu::CaptureHook's
    // doc comment (used by the button-remap pages' "press to bind" flow).
    void SetCaptureHook(std::function<bool(uint32_t *, uint32_t *)> hook) { m_menu.CaptureHook = std::move(hook); }

protected:
    Menu m_menu;
};
