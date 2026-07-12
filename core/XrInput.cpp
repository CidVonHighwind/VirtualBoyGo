#include "XrInput.h"

#include "ui/ButtonMapping.h"

#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

void CheckXr(XrResult result, const char* what) {
    if (XR_FAILED(result)) {
        throw std::runtime_error(std::string("OpenXR call failed: ") + what + " (" + std::to_string(result) + ")");
    }
}

constexpr float kThumbstickDeadzone = 0.5f;

}  // namespace

void XrInput::Initialize(XrInstance instance, XrSession session) {
    m_instance = instance;

    CheckXr(xrStringToPath(instance, "/user/hand/left", &m_leftHandPath), "xrStringToPath (left)");
    CheckXr(xrStringToPath(instance, "/user/hand/right", &m_rightHandPath), "xrStringToPath (right)");
    const XrPath subactionPaths[] = {m_leftHandPath, m_rightHandPath};

    XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
    std::strncpy(actionSetInfo.actionSetName, "menu_input", XR_MAX_ACTION_SET_NAME_SIZE - 1);
    std::strncpy(actionSetInfo.localizedActionSetName, "Menu Input", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE - 1);
    CheckXr(xrCreateActionSet(instance, &actionSetInfo, &m_actionSet), "xrCreateActionSet");

    auto createAction = [&](const char* name, const char* localizedName, XrActionType type, bool bothHands) -> XrAction {
        XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};
        std::strncpy(info.actionName, name, XR_MAX_ACTION_NAME_SIZE - 1);
        std::strncpy(info.localizedActionName, localizedName, XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
        info.actionType = type;
        if (bothHands) {
            info.countSubactionPaths = 2;
            info.subactionPaths = subactionPaths;
        }
        XrAction action = XR_NULL_HANDLE;
        CheckXr(xrCreateAction(m_actionSet, &info, &action), "xrCreateAction");
        return action;
    };

    m_thumbstickAction = createAction("thumbstick", "Thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, true);
    m_aClickAction = createAction("a_click", "A Button", XR_ACTION_TYPE_BOOLEAN_INPUT, false);
    m_bClickAction = createAction("b_click", "B Button", XR_ACTION_TYPE_BOOLEAN_INPUT, false);
    m_xClickAction = createAction("x_click", "X Button", XR_ACTION_TYPE_BOOLEAN_INPUT, false);
    m_yClickAction = createAction("y_click", "Y Button", XR_ACTION_TYPE_BOOLEAN_INPUT, false);
    m_triggerAction = createAction("trigger", "Trigger", XR_ACTION_TYPE_FLOAT_INPUT, true);
    m_menuClickAction = createAction("menu_click", "Menu Button", XR_ACTION_TYPE_BOOLEAN_INPUT, false);

    auto path = [&](const char* p) {
        XrPath result;
        CheckXr(xrStringToPath(instance, p, &result), "xrStringToPath");
        return result;
    };

    XrActionSuggestedBinding bindings[] = {
        {m_thumbstickAction, path("/user/hand/left/input/thumbstick")},
        {m_thumbstickAction, path("/user/hand/right/input/thumbstick")},
        {m_aClickAction, path("/user/hand/right/input/a/click")},
        {m_bClickAction, path("/user/hand/right/input/b/click")},
        // Touch's left controller has X/Y where the right has A/B.
        {m_xClickAction, path("/user/hand/left/input/x/click")},
        {m_yClickAction, path("/user/hand/left/input/y/click")},
        // Index trigger, analog - thresholded to a bool in Sync(). No
        // dedicated /click sub-path for this input on touch_controller.
        {m_triggerAction, path("/user/hand/left/input/trigger/value")},
        {m_triggerAction, path("/user/hand/right/input/trigger/value")},
        // Left controller's dedicated menu button - Touch controllers only
        // have this on the left hand.
        {m_menuClickAction, path("/user/hand/left/input/menu/click")},
    };

    XrPath profilePath;
    CheckXr(xrStringToPath(instance, "/interaction_profiles/oculus/touch_controller", &profilePath),
            "xrStringToPath (profile)");

    XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggestedBindings.interactionProfile = profilePath;
    suggestedBindings.suggestedBindings = bindings;
    suggestedBindings.countSuggestedBindings = static_cast<uint32_t>(std::size(bindings));
    CheckXr(xrSuggestInteractionProfileBindings(instance, &suggestedBindings), "xrSuggestInteractionProfileBindings");

    XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &m_actionSet;
    CheckXr(xrAttachSessionActionSets(session, &attachInfo), "xrAttachSessionActionSets");
}

void XrInput::Shutdown() {
    if (m_actionSet != XR_NULL_HANDLE) {
        xrDestroyActionSet(m_actionSet);
        m_actionSet = XR_NULL_HANDLE;
    }
}

void XrInput::Sync(XrSession session) {
    m_buttonStates[ButtonMapper::DeviceGamepad] = 0;
    m_buttonStates[ButtonMapper::DeviceLeftTouch] = 0;
    m_buttonStates[ButtonMapper::DeviceRightTouch] = 0;

    XrActiveActionSet activeActionSet{m_actionSet, XR_NULL_PATH};
    XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &activeActionSet;
    // Not fatal if this fails (e.g. session not focused yet) - just means
    // button states won't update this frame.
    if (XR_FAILED(xrSyncActions(session, &syncInfo))) {
        return;
    }

    struct HandInfo {
        XrPath path;
        int deviceSlot;
    };
    const HandInfo hands[] = {
        {m_leftHandPath, ButtonMapper::DeviceLeftTouch},
        {m_rightHandPath, ButtonMapper::DeviceRightTouch},
    };

    m_rightThumbstick = {0.0f, 0.0f};
    m_leftThumbstick = {0.0f, 0.0f};
    for (const HandInfo& hand : hands) {
        XrActionStateGetInfo stickGetInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        stickGetInfo.action = m_thumbstickAction;
        stickGetInfo.subactionPath = hand.path;

        XrActionStateVector2f stickState{XR_TYPE_ACTION_STATE_VECTOR2F};
        if (XR_SUCCEEDED(xrGetActionStateVector2f(session, &stickGetInfo, &stickState)) && stickState.isActive) {
            uint32_t& bits = m_buttonStates[hand.deviceSlot];
            if (stickState.currentState.y > kThumbstickDeadzone) bits |= ButtonMapper::ButtonMapping[ButtonMapper::EmuButton_Up];
            if (stickState.currentState.y < -kThumbstickDeadzone) bits |= ButtonMapper::ButtonMapping[ButtonMapper::EmuButton_Down];
            if (stickState.currentState.x < -kThumbstickDeadzone) bits |= ButtonMapper::ButtonMapping[ButtonMapper::EmuButton_Left];
            if (stickState.currentState.x > kThumbstickDeadzone) bits |= ButtonMapper::ButtonMapping[ButtonMapper::EmuButton_Right];
            if (hand.deviceSlot == ButtonMapper::DeviceRightTouch) m_rightThumbstick = stickState.currentState;
            else m_leftThumbstick = stickState.currentState;
        }
    }

    constexpr float kTriggerThreshold = 0.5f;
    auto readTrigger = [&](XrPath handPath) {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = m_triggerAction;
        getInfo.subactionPath = handPath;
        XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
        return XR_SUCCEEDED(xrGetActionStateFloat(session, &getInfo, &state)) && state.isActive &&
               state.currentState > kTriggerThreshold;
    };
    m_leftTriggerPressed = readTrigger(m_leftHandPath);
    m_rightTriggerPressed = readTrigger(m_rightHandPath);

    auto readClick = [&](XrAction action, uint32_t emuButton) {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        getInfo.subactionPath = XR_NULL_PATH;

        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
        if (XR_SUCCEEDED(xrGetActionStateBoolean(session, &getInfo, &state)) && state.isActive && state.currentState) {
            m_buttonStates[ButtonMapper::DeviceRightTouch] |= ButtonMapper::ButtonMapping[emuButton];
            return true;
        }
        return false;
    };

    m_aPressed = readClick(m_aClickAction, ButtonMapper::EmuButton_A);
    m_bPressed = readClick(m_bClickAction, ButtonMapper::EmuButton_B);

    // X/Y have no menu-navigation meaning (unlike A/B), so these don't touch
    // m_buttonStates - just the raw gameplay-mapping state.
    auto readRawBool = [&](XrAction action) {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        getInfo.subactionPath = XR_NULL_PATH;
        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
        return XR_SUCCEEDED(xrGetActionStateBoolean(session, &getInfo, &state)) && state.isActive && state.currentState;
    };
    m_xPressed = readRawBool(m_xClickAction);
    m_yPressed = readRawBool(m_yClickAction);

    m_menuButtonPressed = false;
    XrActionStateGetInfo menuGetInfo{XR_TYPE_ACTION_STATE_GET_INFO};
    menuGetInfo.action = m_menuClickAction;
    menuGetInfo.subactionPath = XR_NULL_PATH;
    XrActionStateBoolean menuState{XR_TYPE_ACTION_STATE_BOOLEAN};
    if (XR_SUCCEEDED(xrGetActionStateBoolean(session, &menuGetInfo, &menuState)) && menuState.isActive) {
        m_menuButtonPressed = menuState.currentState;
    }
}

void XrInput::GetButtonStates(uint32_t buttonStates[3]) const {
    buttonStates[0] = m_buttonStates[0];
    buttonStates[1] = m_buttonStates[1];
    buttonStates[2] = m_buttonStates[2];
}
