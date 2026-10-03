// SPDX-License-Identifier: GPL-2.0-or-later
#include "openxr_session.h"
#include "openxr_input.h"
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace bvb {
using Microsoft::WRL::ComPtr;
namespace {
void xr_check(XrResult result, const char* call) {
    if (result == XR_SESSION_LOSS_PENDING)
        throw std::runtime_error(std::string(call) + ": OpenXR session loss pending; restart the application when the runtime is ready.");
    if (XR_FAILED(result))
        throw std::runtime_error(std::string(call) + " failed (XrResult " + std::to_string(result) + ")");
}
void dx_check(HRESULT result, const char* call) {
    if (FAILED(result))
        throw std::runtime_error(std::string(call) + " failed (HRESULT " + std::to_string(result) + ")");
}
bool same_luid(LUID a, LUID b) { return a.HighPart == b.HighPart && a.LowPart == b.LowPart; }
const char* state_name(XrSessionState state) {
    switch (state) {
    case XR_SESSION_STATE_IDLE: return "IDLE";
    case XR_SESSION_STATE_READY: return "READY";
    case XR_SESSION_STATE_SYNCHRONIZED: return "SYNCHRONIZED";
    case XR_SESSION_STATE_VISIBLE: return "VISIBLE";
    case XR_SESSION_STATE_FOCUSED: return "FOCUSED";
    case XR_SESSION_STATE_STOPPING: return "STOPPING";
    case XR_SESSION_STATE_LOSS_PENDING: return "LOSS_PENDING";
    case XR_SESSION_STATE_EXITING: return "EXITING";
    default: return "UNKNOWN";
    }
}
}

struct OpenXrSession::Impl {
    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId system = XR_NULL_SYSTEM_ID;
    XrSession session = XR_NULL_HANDLE;
    std::unique_ptr<OpenXrInput> controllers;
    XrSpace local_space = XR_NULL_HANDLE;
    XrSpace view_space = XR_NULL_HANDLE;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    struct EyeSwapchain {
        XrSwapchain handle = XR_NULL_HANDLE;
        // Textures are owned by OpenXR: do not AddRef/Release them.
        std::vector<XrSwapchainImageD3D11KHR> images;
    };
    std::array<EyeSwapchain, 2> eyes, ui_eyes;
    unsigned ui_scale=1;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    bool session_running = false;
    bool done = false;
    bool loss = false;
    bool exit_requested = false;
    bool need_anchor = true;
    XrTime reference_change_time = 0;
    ScreenAnchor anchor;
    XrSessionState state = XR_SESSION_STATE_UNKNOWN;

    ~Impl() {
        // Destruction also handles partially completed initialize() calls.
        if (context) { context->ClearState(); context->Flush(); }
        for (auto* pair : {&eyes,&ui_eyes}) for (auto& eye : *pair) {
            if (eye.handle != XR_NULL_HANDLE) xrDestroySwapchain(eye.handle);
            eye.images.clear();
        }
        if (view_space != XR_NULL_HANDLE) xrDestroySpace(view_space);
        if (local_space != XR_NULL_HANDLE) xrDestroySpace(local_space);
        // xrEndSession is legal only in STOPPING. Destroying a running session
        // is allowed, including error/instance-loss cleanup.
        if (session != XR_NULL_HANDLE) xrDestroySession(session);
        controllers.reset();
        context.Reset();
        device.Reset();
        if (instance != XR_NULL_HANDLE) xrDestroyInstance(instance);
    }

    void initialize(bool enable_controllers, unsigned scale) {
        if(scale<1 || scale>4) throw std::invalid_argument("Unsupported UI resolution scale");
        if (instance != XR_NULL_HANDLE) throw std::logic_error("OpenXR session already initialized");
        ui_scale=scale;
        uint32_t count = 0;
        xr_check(xrEnumerateInstanceExtensionProperties(nullptr, 0, &count, nullptr), "Enumerate extensions");
        std::vector<XrExtensionProperties> extensions(count, {XR_TYPE_EXTENSION_PROPERTIES});
        xr_check(xrEnumerateInstanceExtensionProperties(nullptr, count, &count, extensions.data()), "Read extensions");
        const char* extension = XR_KHR_D3D11_ENABLE_EXTENSION_NAME;
        if (std::none_of(extensions.begin(), extensions.end(), [&](const auto& item) {
                return std::strcmp(item.extensionName, extension) == 0;
            })) throw std::runtime_error("Active OpenXR runtime does not support D3D11");
        XrInstanceCreateInfo create{XR_TYPE_INSTANCE_CREATE_INFO};
        strcpy_s(create.applicationInfo.applicationName, "Beetle VB OpenXR");
        create.applicationInfo.applicationVersion = 1;
        create.applicationInfo.apiVersion = XR_API_VERSION_1_0;
        create.enabledExtensionCount = 1;
        create.enabledExtensionNames = &extension;
        xr_check(xrCreateInstance(&create, &instance), "Create instance");
        XrInstanceProperties runtime{XR_TYPE_INSTANCE_PROPERTIES};
        xr_check(xrGetInstanceProperties(instance, &runtime), "Runtime properties");
        std::cout << "Runtime: " << runtime.runtimeName << '\n';
        XrSystemGetInfo system_info{XR_TYPE_SYSTEM_GET_INFO};
        system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        xr_check(xrGetSystem(instance, &system_info, &system), "Find headset");
        XrSystemProperties system_properties{XR_TYPE_SYSTEM_PROPERTIES};
        xr_check(xrGetSystemProperties(instance, system, &system_properties), "Headset properties");
        std::cout << "Headset: " << system_properties.systemName << '\n';
        if (system_properties.graphicsProperties.maxLayerCount < 2 ||
            system_properties.graphicsProperties.maxSwapchainImageWidth < eye_width*ui_scale ||
            system_properties.graphicsProperties.maxSwapchainImageHeight < eye_height*ui_scale)
            throw std::runtime_error("Headset cannot support the requested native/UI quad textures");

        xr_check(xrEnumerateViewConfigurations(instance, system, 0, &count, nullptr), "Count view configurations");
        std::vector<XrViewConfigurationType> views(count);
        xr_check(xrEnumerateViewConfigurations(instance, system, count, &count, views.data()), "Read view configurations");
        if (std::find(views.begin(), views.end(), XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) == views.end())
            throw std::runtime_error("Headset does not expose PRIMARY_STEREO views");
        xr_check(xrEnumerateEnvironmentBlendModes(instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            0, &count, nullptr), "Count blend modes");
        std::vector<XrEnvironmentBlendMode> blend_modes(count);
        xr_check(xrEnumerateEnvironmentBlendModes(instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            count, &count, blend_modes.data()), "Read blend modes");
        if (std::find(blend_modes.begin(), blend_modes.end(), XR_ENVIRONMENT_BLEND_MODE_OPAQUE) == blend_modes.end())
            throw std::runtime_error("This PC VR test requires an opaque environment blend mode");

        PFN_xrVoidFunction function = nullptr;
        xr_check(xrGetInstanceProcAddr(instance, "xrGetD3D11GraphicsRequirementsKHR", &function), "D3D11 requirements function");
        XrGraphicsRequirementsD3D11KHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
        xr_check(reinterpret_cast<PFN_xrGetD3D11GraphicsRequirementsKHR>(function)(instance, system, &requirements), "D3D11 requirements");
        ComPtr<IDXGIFactory1> factory;
        dx_check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "Create DXGI factory");
        ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0; ; ++i) {
            ComPtr<IDXGIAdapter1> candidate;
            HRESULT result = factory->EnumAdapters1(i, &candidate);
            if (result == DXGI_ERROR_NOT_FOUND) break;
            dx_check(result, "Enumerate graphics adapter");
            DXGI_ADAPTER_DESC1 desc{};
            dx_check(candidate->GetDesc1(&desc), "Graphics adapter description");
            if (same_luid(desc.AdapterLuid, requirements.adapterLuid)) {
                adapter = candidate;
                break;
            }
        }
        if (!adapter) throw std::runtime_error("OpenXR's selected graphics adapter was not found");
        const std::array<D3D_FEATURE_LEVEL, 4> preferred = {
            D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
        std::vector<D3D_FEATURE_LEVEL> levels;
        for (auto level : preferred) if (level >= requirements.minFeatureLevel) levels.push_back(level);
        if (levels.empty()) throw std::runtime_error("OpenXR requests an unsupported D3D feature level");
        D3D_FEATURE_LEVEL selected{};
        dx_check(D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels.data(), static_cast<UINT>(levels.size()),
            D3D11_SDK_VERSION, &device, &selected, &context), "Create runtime-selected D3D11 device");
        std::cout << "D3D11 adapter LUID: " << requirements.adapterLuid.HighPart << ':'
                  << requirements.adapterLuid.LowPart << ", feature level: " << selected << '\n';
        XrGraphicsBindingD3D11KHR binding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
        binding.device = device.Get();
        XrSessionCreateInfo session_create{XR_TYPE_SESSION_CREATE_INFO};
        session_create.next = &binding;
        session_create.systemId = system;
        xr_check(xrCreateSession(instance, &session_create, &session), "Create session");
        if (enable_controllers) {
            controllers = std::make_unique<OpenXrInput>();
            controllers->initialize(instance, session);
        }

        xr_check(xrEnumerateReferenceSpaces(session, 0, &count, nullptr), "Count reference spaces");
        std::vector<XrReferenceSpaceType> spaces(count);
        xr_check(xrEnumerateReferenceSpaces(session, count, &count, spaces.data()), "Read reference spaces");
        for (auto type : {XR_REFERENCE_SPACE_TYPE_LOCAL, XR_REFERENCE_SPACE_TYPE_VIEW})
            if (std::find(spaces.begin(), spaces.end(), type) == spaces.end())
                throw std::runtime_error("OpenXR requires LOCAL and VIEW reference spaces for this screen");
        XrReferenceSpaceCreateInfo space_create{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        space_create.poseInReferenceSpace.orientation.w = 1;
        space_create.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        xr_check(xrCreateReferenceSpace(session, &space_create, &local_space), "Create LOCAL space");
        space_create.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
        xr_check(xrCreateReferenceSpace(session, &space_create, &view_space), "Create VIEW space");

        xr_check(xrEnumerateSwapchainFormats(session, 0, &count, nullptr), "Count swapchain formats");
        std::vector<int64_t> formats(count);
        xr_check(xrEnumerateSwapchainFormats(session, count, &count, formats.data()), "Read swapchain formats");
        // Pattern bytes are sRGB. Require an sRGB format to preserve their colors.
        for (auto desired : {DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB})
            if (std::find(formats.begin(), formats.end(), int64_t(desired)) != formats.end()) {
                format = desired;
                break;
            }
        if (format == DXGI_FORMAT_UNKNOWN) throw std::runtime_error("Runtime provides no supported RGBA/BGRA sRGB swapchain format");
        for (auto* pair : {&eyes,&ui_eyes}) {
            if (pair == &ui_eyes && ui_scale == 1) continue;
            const auto pair_scale = pair == &ui_eyes ? ui_scale : 1;
            for (auto& eye : *pair) {
                XrSwapchainCreateInfo swapchain_create{XR_TYPE_SWAPCHAIN_CREATE_INFO};
                swapchain_create.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
                swapchain_create.format = format;
                swapchain_create.sampleCount = 1;
                swapchain_create.width = eye_width*pair_scale;
                swapchain_create.height = eye_height*pair_scale;
                swapchain_create.faceCount = 1;
                swapchain_create.arraySize = 1;
                swapchain_create.mipCount = 1;
                xr_check(xrCreateSwapchain(session, &swapchain_create, &eye.handle), "Create eye swapchain");
                xr_check(xrEnumerateSwapchainImages(eye.handle, 0, &count, nullptr), "Count eye images");
                if (!count) throw std::runtime_error("Runtime returned an empty eye swapchain");
                eye.images.resize(count, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
                xr_check(xrEnumerateSwapchainImages(eye.handle, count, &count,
                    reinterpret_cast<XrSwapchainImageBaseHeader*>(eye.images.data())), "Read eye images");
            }
        }
        std::cout << "Native 384x224 eye swapchains created; UI scale " << ui_scale << ". Waiting for READY.\n";
    }

    void poll_events() {
        XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
        for (;;) {
            const auto result = xrPollEvent(instance, &event);
            if (result == XR_EVENT_UNAVAILABLE) break;
            xr_check(result, "Poll events");
            if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
                std::cerr << "OpenXR instance loss pending. Restart after the runtime is ready.\n";
                loss = done = true;
                session_running = false;
            } else if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                const auto& changed = *reinterpret_cast<const XrEventDataSessionStateChanged*>(&event);
                if (changed.session == session) {
                    state = changed.state;
                    std::cout << "Session: " << state_name(state) << '\n';
                    if (state == XR_SESSION_STATE_READY && !session_running && !exit_requested) {
                        XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};
                        begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                        xr_check(xrBeginSession(session, &begin), "Begin session");
                        session_running = true;
                        need_anchor = true;
                    } else if (state == XR_SESSION_STATE_STOPPING) {
                        if (session_running) xr_check(xrEndSession(session), "End session");
                        session_running = false;
                        if (exit_requested) done = true;
                    } else if (state == XR_SESSION_STATE_EXITING || state == XR_SESSION_STATE_LOSS_PENDING) {
                        done = true;
                        session_running = false;
                        loss = state == XR_SESSION_STATE_LOSS_PENDING;
                    }
                }
            } else if (event.type == XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) {
                const auto& changed = *reinterpret_cast<const XrEventDataReferenceSpaceChangePending*>(&event);
                if (changed.session == session && changed.referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL) {
                    // Reanchor only after this predicted-time reference change takes effect.
                    need_anchor = true;
                    reference_change_time = changed.changeTime;
                }
            } else if (event.type == XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED) {
                const auto& changed = *reinterpret_cast<const XrEventDataInteractionProfileChanged*>(&event);
                if (controllers && changed.session == session) controllers->profiles_changed();
            } else if (event.type == XR_TYPE_EVENT_DATA_EVENTS_LOST) {
                std::cerr << "OpenXR reported lost events.\n";
            }
            if (done) break;
            event = {XR_TYPE_EVENT_DATA_BUFFER};
        }
    }

    void request_exit() {
        if (exit_requested || done) return;
        exit_requested = true;
        if (session_running) xr_check(xrRequestExitSession(session), "Request session exit");
        else done = true;
    }

    void upload(EyeSwapchain& eye, const std::vector<std::uint8_t>& pixels, unsigned width) {
        XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        uint32_t index = 0;
        xr_check(xrAcquireSwapchainImage(eye.handle, &acquire, &index), "Acquire eye image");
        XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        wait.timeout = 1000000000; // One second; abort rather than upload to an unavailable image.
        const auto waited = xrWaitSwapchainImage(eye.handle, &wait);
        if (waited == XR_TIMEOUT_EXPIRED)
            throw std::runtime_error("Timed out waiting for an eye image; closing the session without writing to it.");
        xr_check(waited, "Wait eye image");
        XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        try {
            if (index >= eye.images.size() || !eye.images[index].texture)
                throw std::runtime_error("Runtime returned an invalid eye texture");
            if (format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
                const auto bgra = rgba_to_bgra(pixels);
                context->UpdateSubresource(eye.images[index].texture, 0, nullptr, bgra.data(), width * 4, 0);
            } else context->UpdateSubresource(eye.images[index].texture, 0, nullptr, pixels.data(), width * 4, 0);
            // Submit graphics commands before handing the texture back to OpenXR.
            context->Flush();
            dx_check(device->GetDeviceRemovedReason(), "D3D11 device health");
        } catch (...) {
            xrReleaseSwapchainImage(eye.handle, &release);
            throw;
        }
        xr_check(xrReleaseSwapchainImage(eye.handle, &release), "Release eye image");
    }

    void render_frame(const StereoFrame& frame, const ScreenSettings& input_settings) {
        if (!session_running || done) return;
        if(!valid_stereo_frame(frame)) throw std::invalid_argument("Screen requires two complete RGBA images");
        const bool native=frame.width==eye_width && frame.height==eye_height;
        if(!native && (frame.width!=eye_width*ui_scale || frame.height!=eye_height*ui_scale))
            throw std::invalid_argument("Screen frame does not match its swapchain dimensions");
        auto& textures=native?eyes:ui_eyes;
        ScreenSettings settings = input_settings;
        settings.clamp();
        XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState frame_state{XR_TYPE_FRAME_STATE};
        xr_check(xrWaitFrame(session, &wait, &frame_state), "Wait frame");
        XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};
        xr_check(xrBeginFrame(session, &begin), "Begin frame");
        XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
        end.displayTime = frame_state.predictedDisplayTime;
        end.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        std::array<XrCompositionLayerQuad, 2> layers{};
        std::array<const XrCompositionLayerBaseHeader*, 2> headers{};
        try {
            bool located = !need_anchor;
            if (need_anchor && frame_state.shouldRender && frame_state.predictedDisplayTime >= reference_change_time) {
                XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
                xr_check(xrLocateSpace(view_space, local_space, frame_state.predictedDisplayTime, &location), "Locate head for recenter");
                constexpr auto valid = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;
                if ((location.locationFlags & valid) == valid) {
                    const auto& q = location.pose.orientation;
                    anchor = {location.pose.position.x, location.pose.position.y, location.pose.position.z,
                        std::atan2(2 * (q.w * q.y + q.x * q.z), 1 - 2 * (q.x * q.x + q.y * q.y))};
                    need_anchor = false;
                    located = true;
                    std::cout << "Screen recentered.\n";
                }
            }
            if (frame_state.shouldRender && located && !exit_requested) {
                upload(textures[0], settings.swap_eyes ? frame.right : frame.left,frame.width);
                upload(textures[1], settings.swap_eyes ? frame.left : frame.right,frame.width);
                const auto pose = screen_pose(anchor, settings);
                for (unsigned eye = 0; eye < 2; ++eye) {
                    auto& layer = layers[eye];
                    layer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
                    layer.space = local_space;
                    layer.eyeVisibility = eye ? XR_EYE_VISIBILITY_RIGHT : XR_EYE_VISIBILITY_LEFT;
                    layer.subImage.swapchain = textures[eye].handle;
                    layer.subImage.imageRect.extent = {int32_t(frame.width), int32_t(frame.height)};
                    layer.pose.orientation = {pose.orientation[0], pose.orientation[1], pose.orientation[2], pose.orientation[3]};
                    layer.pose.position = {pose.position[0], pose.position[1], pose.position[2]};
                    layer.size = {settings.width, settings.height()};
                    headers[eye] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer);
                }
                end.layerCount = 2;
                end.layers = headers.data();
            }
        } catch (...) {
            end.layerCount = 0;
            end.layers = nullptr;
            xrEndFrame(session, &end);
            throw;
        }
        xr_check(xrEndFrame(session, &end), "End frame");
    }
};

OpenXrSession::OpenXrSession() : impl(std::make_unique<Impl>()) {}
OpenXrSession::~OpenXrSession() = default;
void OpenXrSession::initialize(bool enable_controllers,unsigned ui_scale) { impl->initialize(enable_controllers,ui_scale); }
VrInput OpenXrSession::poll_input(float deadzone) {
    return impl->controllers ? impl->controllers->poll(focused(), deadzone) : VrInput{};
}
void OpenXrSession::poll_events() { impl->poll_events(); }
bool OpenXrSession::running() const { return impl->session_running; }
bool OpenXrSession::focused() const { return impl->session_running && impl->state == XR_SESSION_STATE_FOCUSED && !impl->done; }
bool OpenXrSession::finished() const { return impl->done; }
bool OpenXrSession::lost() const { return impl->loss; }
void OpenXrSession::request_exit() { impl->request_exit(); }
void OpenXrSession::recenter() { impl->need_anchor = true; }
void OpenXrSession::render_frame(const StereoFrame& frame, const ScreenSettings& settings) { impl->render_frame(frame, settings); }
}
