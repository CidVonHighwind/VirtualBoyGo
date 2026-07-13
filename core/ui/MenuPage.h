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
    // "Resume", not wherever the user was before loading a ROM).
    void ResetSelection() { m_menu.ResetSelection(); }

protected:
    Menu m_menu;
};
