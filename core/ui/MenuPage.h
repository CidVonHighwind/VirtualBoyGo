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
    // this page becomes current. Virtual so MainPage can also refresh its
    // save preview here.
    virtual void ResetSelection() { m_menu.ResetSelection(); }

    // Whether this page has anywhere for B (or A, if swapped) to go - drives
    // the bottom-bar "Back" hint (see AppMenu::RenderContent). MainPage has
    // no BackPress (it's the root), every other page navigates back to it.
    bool HasBackAction() const { return static_cast<bool>(m_menu.BackPress); }

    // Suspends normal navigation to capture the next raw button press - see
    // Menu::CaptureHook.
    void SetCaptureHook(std::function<bool(uint32_t *, uint32_t *)> hook) { m_menu.CaptureHook = std::move(hook); }
    void SetRawCaptureHook(std::function<void(const ButtonMapper::MappedButton &)> hook) { m_menu.RawCaptureHook = std::move(hook); }
    void SubmitRawCaptureInput(const ButtonMapper::MappedButton &button) { m_menu.SubmitRawCaptureInput(button); }

protected:
    Menu m_menu;
};
