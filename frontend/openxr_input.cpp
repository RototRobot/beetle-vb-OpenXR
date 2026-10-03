// SPDX-License-Identifier: GPL-2.0-or-later
#include "openxr_input.h"
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>

namespace bvb {
namespace {
void check(XrResult result, const char* call) {
    if (XR_FAILED(result) || result == XR_SESSION_LOSS_PENDING)
        throw std::runtime_error(std::string(call) + " failed (XrResult " + std::to_string(result) + ")");
}
XrActionType type(VrActionType value) {
    switch (value) {
    case VrActionType::vector2: return XR_ACTION_TYPE_VECTOR2F_INPUT;
    case VrActionType::scalar: return XR_ACTION_TYPE_FLOAT_INPUT;
    default: return XR_ACTION_TYPE_BOOLEAN_INPUT;
    }
}
}
OpenXrInput::~OpenXrInput() {
    // Destroying the set also destroys its action handles. Instance still lives.
    if (action_set != XR_NULL_HANDLE) xrDestroyActionSet(action_set);
}
XrPath OpenXrInput::path(const char* text) {
    XrPath result = XR_NULL_PATH;
    check(xrStringToPath(instance, text, &result), "Resolve controller path"); return result;
}
void OpenXrInput::initialize(XrInstance xr_instance, XrSession xr_session) {
    if (instance != XR_NULL_HANDLE) throw std::logic_error("Controller input already initialized");
    instance = xr_instance; session = xr_session;
    hands = {path("/user/hand/left"), path("/user/hand/right")};
    XrActionSetCreateInfo set_info{XR_TYPE_ACTION_SET_CREATE_INFO};
    std::snprintf(set_info.actionSetName, sizeof(set_info.actionSetName), "%s", "virtual_boy");
    std::snprintf(set_info.localizedActionSetName, sizeof(set_info.localizedActionSetName), "%s", "Virtual Boy controls");
    check(xrCreateActionSet(instance, &set_info, &action_set), "Create controller action set");
    for (std::size_t i = 0; i < actions.size(); ++i) {
        XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};
        std::snprintf(info.actionName, sizeof(info.actionName), "%s", vr_actions[i].name);
        std::snprintf(info.localizedActionName, sizeof(info.localizedActionName), "%s", vr_actions[i].label);
        info.actionType = type(vr_actions[i].type);
        info.countSubactionPaths = static_cast<uint32_t>(hands.size()); info.subactionPaths = hands.data();
        check(xrCreateAction(action_set, &info, &actions[i]), "Create controller action");
    }
    for (const auto& profile : vr_profiles()) {
        std::vector<XrActionSuggestedBinding> bindings;
        for (const auto& binding : profile.bindings)
            bindings.push_back({actions[static_cast<std::size_t>(binding.action)], path(binding.component)});
        XrInteractionProfileSuggestedBinding info{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        info.interactionProfile = path(profile.path);
        info.countSuggestedBindings = static_cast<uint32_t>(bindings.size()); info.suggestedBindings = bindings.data();
        const auto result = xrSuggestInteractionProfileBindings(instance, &info);
        if (result == XR_ERROR_PATH_UNSUPPORTED) {
            std::cerr << "Runtime rejected controller profile: " << profile.path << '\n'; continue;
        }
        check(result, "Suggest controller bindings");
    }
    XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attach.countActionSets = 1; attach.actionSets = &action_set;
    check(xrAttachSessionActionSets(session, &attach), "Attach controller action set");
    std::cout << "OpenXR controller actions attached. Both stick/pad clicks toggle settings.\n";
}
XrActionStateBoolean OpenXrInput::boolean(VrAction action, XrPath hand) {
    XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};
    info.action = actions[static_cast<std::size_t>(action)]; info.subactionPath = hand;
    XrActionStateBoolean value{XR_TYPE_ACTION_STATE_BOOLEAN};
    check(xrGetActionStateBoolean(session, &info, &value), "Read controller button");
    if (!value.isActive) value.currentState = XR_FALSE;
    return value;
}
VrInput OpenXrInput::poll(bool focused, float deadzone) {
    if (!focused) { mapper.reset(); return {}; }
    XrActiveActionSet active{action_set, XR_NULL_PATH};
    XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO}; sync.countActiveActionSets = 1; sync.activeActionSets = &active;
    const auto synced = xrSyncActions(session, &sync);
    if (synced == XR_SESSION_NOT_FOCUSED) { mapper.reset(); return {}; }
    check(synced, "Synchronize controller actions");
    std::array<VrHandInput, 2> inputs;
    std::array<bool, 2> available{};
    for (std::size_t i = 0; i < hands.size(); ++i) {
        auto& input = inputs[i];
        XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};
        info.subactionPath = hands[i]; info.action = actions[static_cast<std::size_t>(VrAction::direction)];
        XrActionStateVector2f axis{XR_TYPE_ACTION_STATE_VECTOR2F};
        check(xrGetActionStateVector2f(session, &info, &axis), "Read controller pad");
        input.axis_active = axis.isActive != XR_FALSE;
        if (input.axis_active) { input.x = axis.currentState.x; input.y = axis.currentState.y; }
        const auto touch = boolean(VrAction::axis_touch, hands[i]);
        input.axis_contact = !touch.isActive || touch.currentState;
        info.action = actions[static_cast<std::size_t>(VrAction::trigger)];
        XrActionStateFloat trigger{XR_TYPE_ACTION_STATE_FLOAT};
        check(xrGetActionStateFloat(session, &info, &trigger), "Read controller trigger");
        input.trigger_active = trigger.isActive != XR_FALSE;
        if (input.trigger_active) input.trigger = trigger.currentState;
        available[i] = input.axis_active || input.trigger_active || touch.isActive;
        info.action=actions[static_cast<std::size_t>(VrAction::grip)];
        XrActionStateFloat grip{XR_TYPE_ACTION_STATE_FLOAT};
        check(xrGetActionStateFloat(session,&info,&grip),"Read controller grip");
        input.grip_active=grip.isActive != XR_FALSE;
        if(input.grip_active) input.grip=grip.currentState;
        available[i]=available[i] || input.grip_active;
        auto pressed = [&](VrAction action) {
            const auto value = boolean(action, hands[i]);
            available[i] = available[i] || value.isActive;
            return value.currentState != XR_FALSE;
        };
        input.a = pressed(VrAction::a); input.b = pressed(VrAction::b);
        input.start = pressed(VrAction::start); input.select = pressed(VrAction::select);
        input.menu_click = pressed(VrAction::menu_click);
    }
    // Some runtimes retain the same profile across disconnect/reconnect.
    // Action availability changes must still clear held state and require release.
    if (available != last_active) { mapper.reset(); last_active = available; }
    return mapper.update(inputs, true, deadzone);
}
void OpenXrInput::profiles_changed() {
    mapper.reset();
    for (std::size_t i = 0; i < hands.size(); ++i) {
        XrInteractionProfileState profile{XR_TYPE_INTERACTION_PROFILE_STATE};
        check(xrGetCurrentInteractionProfile(session, hands[i], &profile), "Read controller profile");
        std::string name = "none";
        if (profile.interactionProfile != XR_NULL_PATH) {
            uint32_t size = 0;
            check(xrPathToString(instance, profile.interactionProfile, 0, &size, nullptr), "Count profile path");
            name.resize(size);
            check(xrPathToString(instance, profile.interactionProfile, size, &size, name.data()), "Read profile path");
            if (!name.empty() && name.back() == '\0') name.pop_back();
        }
        std::cout << "Controller " << (i ? "right" : "left") << " profile: " << name << '\n';
    }
}
}
