#pragma once

#include <openxr/openxr.h>

#include <cstdint>

// Polls OpenXR controller input two ways: into a ButtonMapping-compatible
// uint[3] bitmask (see core/ui/ButtonMapping.h) for menu navigation (ported
// from FrontendGo's ~230-line VrApi controller-enumeration subsystem in the
// original MenuGo), and via raw per-control getters (GetLeftThumbstick,
// IsAPressed, etc.) that OpenXrApp maps to VB gameplay buttons (see
// Emulator.h's VBButtonBit) - two different semantics from the same Quest
// Touch controller profile, so this class exposes both rather than picking
// one. The DeviceGamepad slot (menu-nav bitmask) is left unused for now.
class XrInput
{
public:
    void Initialize(XrInstance instance, XrSession session);
    void Shutdown();

    // Must be called once per frame (after xrWaitFrame) before reading state.
    void Sync(XrSession session);

    void GetButtonStates(uint32_t buttonStates[3]) const;

    // Left controller's dedicated menu ("hamburger") button, or left stick
    // click as a runtime-independent alternative - separate from
    // buttonStates (the menu-navigation/emulator button set) since this is
    // an app-level "show the menu" gesture, not something any MenuPage
    // reacts to. Instantaneous state (not edge-detected) - callers wanting a
    // single toggle-per-press do their own last-frame comparison, same as
    // buttonStates/lastButtonStates elsewhere.
    bool IsMenuButtonPressed() const { return m_menuButtonPressed; }

    // Raw right-hand thumbstick/A/B state, for gameplay input mapping
    // (OpenXrApp maps this to VB buttons - see Emulator.h's VBButtonBit) -
    // separate from the menu-navigation bitmask buttonStates already
    // encodes these into, since VB button semantics differ (two D-pads) and
    // this class shouldn't need to know about Emulator's bit layout.
    XrVector2f GetRightThumbstick() const { return m_rightThumbstick; }
    bool IsAPressed() const { return m_aPressed; }
    bool IsBPressed() const { return m_bPressed; }

    // Left-hand equivalents - left thumbstick, X/Y buttons (Touch's left
    // controller has these instead of A/B), and both hands' index triggers.
    // Same "raw state for gameplay mapping" purpose as the right-hand
    // getters above.
    XrVector2f GetLeftThumbstick() const { return m_leftThumbstick; }
    bool IsXPressed() const { return m_xPressed; }
    bool IsYPressed() const { return m_yPressed; }
    bool IsLeftTriggerPressed() const { return m_leftTriggerPressed; }
    bool IsRightTriggerPressed() const { return m_rightTriggerPressed; }

private:
    XrInstance m_instance{XR_NULL_HANDLE};
    XrActionSet m_actionSet{XR_NULL_HANDLE};
    XrAction m_thumbstickAction{XR_NULL_HANDLE};
    XrAction m_aClickAction{XR_NULL_HANDLE};
    XrAction m_bClickAction{XR_NULL_HANDLE};
    XrAction m_xClickAction{XR_NULL_HANDLE};
    XrAction m_yClickAction{XR_NULL_HANDLE};
    XrAction m_triggerAction{XR_NULL_HANDLE};
    XrAction m_menuClickAction{XR_NULL_HANDLE};
    XrAction m_thumbstickClickAction{XR_NULL_HANDLE};
    XrPath m_leftHandPath{XR_NULL_PATH};
    XrPath m_rightHandPath{XR_NULL_PATH};

    // Populated by Sync() (which has the session handle the action-state
    // queries need) and simply copied out by GetButtonStates().
    uint32_t m_buttonStates[3]{};
    bool m_menuButtonPressed{false};
    XrVector2f m_rightThumbstick{0.0f, 0.0f};
    bool m_aPressed{false};
    bool m_bPressed{false};
    XrVector2f m_leftThumbstick{0.0f, 0.0f};
    bool m_xPressed{false};
    bool m_yPressed{false};
    bool m_leftTriggerPressed{false};
    bool m_rightTriggerPressed{false};
};
