#pragma once

#include <openxr/openxr.h>

#include <cstdint>

// Polls OpenXR controller input into a ButtonMapping-compatible uint[3]
// bitmask (see core/ui/ButtonMapping.h) - the abstract input contract the
// ported menu navigation logic runs on (replaces FrontendGo's ~230-line
// VrApi controller-enumeration subsystem in the original MenuGo). Targets
// the Quest Touch controller profile: right-hand thumbstick + A/B, left-hand
// thumbstick, mapped into the DeviceLeftTouch/DeviceRightTouch device slots.
// The DeviceGamepad slot is left unused for now.
class XrInput {
   public:
    void Initialize(XrInstance instance, XrSession session);
    void Shutdown();

    // Must be called once per frame (after xrWaitFrame) before reading state.
    void Sync(XrSession session);

    void GetButtonStates(uint32_t buttonStates[3]) const;

   private:
    XrInstance m_instance{XR_NULL_HANDLE};
    XrActionSet m_actionSet{XR_NULL_HANDLE};
    XrAction m_thumbstickAction{XR_NULL_HANDLE};
    XrAction m_aClickAction{XR_NULL_HANDLE};
    XrAction m_bClickAction{XR_NULL_HANDLE};
    XrPath m_leftHandPath{XR_NULL_PATH};
    XrPath m_rightHandPath{XR_NULL_PATH};

    // Populated by Sync() (which has the session handle the action-state
    // queries need) and simply copied out by GetButtonStates().
    uint32_t m_buttonStates[3]{};
};
