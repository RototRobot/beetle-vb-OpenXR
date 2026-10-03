// SPDX-License-Identifier: GPL-2.0-or-later
// Fake OpenXR entry points exercise the real adapter without a runtime/device.
#include "openxr_input.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T handle(std::uintptr_t value) { return reinterpret_cast<T>(value); }
struct Runtime {
    bool attached = false, inactive = false, reject_profile = false;
    int created = 0, destroyed = 0, syncs = 0, reads = 0, suggestions = 0, fail_action = -1;
    XrResult sync_result = XR_SUCCESS;
    std::map<std::string, XrPath> paths;
    std::map<XrAction, bvb::VrAction> actions;
    std::array<bvb::VrHandInput,2> hands{};
} runtime;
std::size_t hand_index(XrPath path) {
    require(path == runtime.paths.at("/user/hand/left") || path == runtime.paths.at("/user/hand/right"), "Invalid subaction hand");
    return path == runtime.paths.at("/user/hand/right") ? 1 : 0;
}
}
extern "C" {
XRAPI_ATTR XrResult XRAPI_CALL xrStringToPath(XrInstance, const char* value, XrPath* result) {
    auto& path = runtime.paths[value]; if (!path) path = runtime.paths.size(); *result = path; return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrCreateActionSet(XrInstance, const XrActionSetCreateInfo* info, XrActionSet* result) {
    require(info->type == XR_TYPE_ACTION_SET_CREATE_INFO && info->actionSetName[0], "Malformed action-set create info");
    *result = handle<XrActionSet>(10); return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrDestroyActionSet(XrActionSet set) {
    require(set == handle<XrActionSet>(10), "Wrong action set destroyed"); ++runtime.destroyed; return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrCreateAction(XrActionSet, const XrActionCreateInfo* info, XrAction* result) {
    require(!runtime.attached && info->countSubactionPaths == 2, "Actions created after attachment or without hands");
    if (runtime.created == runtime.fail_action) return XR_ERROR_RUNTIME_FAILURE;
    for (std::size_t i = 0; i < static_cast<std::size_t>(bvb::VrAction::count); ++i) {
        if (std::strcmp(info->actionName, bvb::vr_actions[i].name)) continue;
        *result = handle<XrAction>(20+i); runtime.actions[*result] = static_cast<bvb::VrAction>(i);
        ++runtime.created; return XR_SUCCESS;
    }
    return XR_ERROR_NAME_INVALID;
}
XRAPI_ATTR XrResult XRAPI_CALL xrSuggestInteractionProfileBindings(XrInstance, const XrInteractionProfileSuggestedBinding* info) {
    require(!runtime.attached && info->countSuggestedBindings > 0, "Suggestions missing or made after attachment");
    ++runtime.suggestions;
    return runtime.reject_profile && runtime.suggestions == 1 ? XR_ERROR_PATH_UNSUPPORTED : XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrAttachSessionActionSets(XrSession, const XrSessionActionSetsAttachInfo* info) {
    require(!runtime.attached && info->countActionSets == 1 && runtime.created == int(bvb::VrAction::count), "Invalid attachment order");
    runtime.attached = true; return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrSyncActions(XrSession, const XrActionsSyncInfo* info) {
    require(runtime.attached && info->countActiveActionSets == 1, "Sync before attachment");
    ++runtime.syncs; return runtime.sync_result;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateBoolean(XrSession, const XrActionStateGetInfo* info, XrActionStateBoolean* state) {
    ++runtime.reads; const auto& hand = runtime.hands[hand_index(info->subactionPath)];
    state->isActive = !runtime.inactive;
    switch(runtime.actions.at(info->action)) {
    case bvb::VrAction::axis_touch: state->isActive = hand.axis_active && !runtime.inactive; state->currentState = hand.axis_contact; break;
    case bvb::VrAction::a: state->currentState = hand.a; break;
    case bvb::VrAction::b: state->currentState = hand.b; break;
    case bvb::VrAction::start: state->currentState = hand.start; break;
    case bvb::VrAction::select: state->currentState = hand.select; break;
    case bvb::VrAction::menu_click: state->currentState = hand.menu_click; break;
    default: return XR_ERROR_ACTION_TYPE_MISMATCH;
    }
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateVector2f(XrSession, const XrActionStateGetInfo* info, XrActionStateVector2f* state) {
    ++runtime.reads; const auto& hand = runtime.hands[hand_index(info->subactionPath)];
    require(runtime.actions.at(info->action) == bvb::VrAction::direction, "Vector query on wrong action");
    state->isActive = hand.axis_active && !runtime.inactive; state->currentState = {hand.x, hand.y}; return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateFloat(XrSession, const XrActionStateGetInfo* info, XrActionStateFloat* state) {
    ++runtime.reads; const auto& hand = runtime.hands[hand_index(info->subactionPath)];
    const auto action=runtime.actions.at(info->action);
    require(action == bvb::VrAction::trigger || action == bvb::VrAction::grip, "Float query on wrong action");
    state->isActive = (action==bvb::VrAction::grip?hand.grip_active:hand.trigger_active) && !runtime.inactive;
    state->currentState = action==bvb::VrAction::grip?hand.grip:hand.trigger; return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetCurrentInteractionProfile(XrSession, XrPath, XrInteractionProfileState* state) {
    state->interactionProfile = XR_NULL_PATH; return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrPathToString(XrInstance, XrPath, uint32_t capacity, uint32_t* size, char* value) {
    *size = 5; if (capacity >= 5) std::memcpy(value, "test",5); return XR_SUCCESS;
}
}
int main() {
    try {
        {
            bvb::OpenXrInput input; runtime.reject_profile = true;
            input.initialize(handle<XrInstance>(1),handle<XrSession>(2));
            require(runtime.attached && runtime.suggestions == 4, "Unsupported suggestion prevented other profiles/attachment");
            input.poll(false); require(runtime.syncs==0, "Unfocused input synchronized actions");
            runtime.hands[0].grip_active=true;
            input.poll(true); // Neutral sample arms input.
            runtime.hands[1].a = true; runtime.hands[0].axis_active = true; runtime.hands[0].x = -1;
            runtime.hands[0].grip=.8f;
            auto result = input.poll(true);
            require(result.sources[bvb::InputSource::vr_l_grip], "Adapter lost remappable grip state");
            require(result.game[bvb::Button::a] && result.game[bvb::Button::left_left], "Adapter lost per-hand action values");
            runtime.hands[0].x = -.2f;
            require(!input.poll(true,.4f).game[bvb::Button::left_left], "Adapter ignored configured deadzone");
            require(input.poll(true,.1f).game[bvb::Button::left_left], "Live deadzone change did not reach the mapper");
            runtime.inactive = true;
            require(!input.poll(true).held, "Inactive runtime values leaked into game input");
            runtime.inactive = false;
            require(!input.poll(true).held, "Reconnected actions accepted held controls before release");
            runtime.hands = {}; input.poll(true);
            runtime.hands[1].a = true; require(input.poll(true).game[bvb::Button::a], "Reconnect release did not rearm");
            runtime.sync_result = XR_SESSION_NOT_FOCUSED;
            const int reads = runtime.reads;
            require(!input.poll(true).held && runtime.reads==reads, "Not-focused sync queried stale action states");
            runtime.sync_result = XR_SUCCESS;
            require(!input.poll(true).held, "Recovered focus accepted held controls before release");
            runtime.hands = {}; input.poll(true);
            runtime.hands[1].a = true; require(input.poll(true).game[bvb::Button::a], "Release did not rearm adapter");
            input.profiles_changed(); require(!input.poll(true).held, "Interaction-profile change retained controls");
            runtime.sync_result = XR_ERROR_RUNTIME_FAILURE;
            bool sync_failed = false;
            try { input.poll(true); } catch (const std::runtime_error&) { sync_failed = true; }
            require(sync_failed, "Failed action synchronization was ignored");
        }
        require(runtime.destroyed==1, "Action set not destroyed exactly once");
        runtime = {}; runtime.fail_action = 3;
        {
            bvb::OpenXrInput input; bool rejected = false;
            try { input.initialize(handle<XrInstance>(1),handle<XrSession>(2)); }
            catch(const std::runtime_error&) { rejected = true; }
            require(rejected && !runtime.attached, "Partial action creation failure was attached");
        }
        require(runtime.destroyed==1, "Partial setup leaked action set/child actions");
        std::cout << "OpenXR action ordering, unsupported profiles, active states, focus and partial cleanup passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
