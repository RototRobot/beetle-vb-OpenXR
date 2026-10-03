// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef _WIN32
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#endif
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <string>

static void check(XrResult result, const char* call) {
    if (XR_FAILED(result)) throw std::runtime_error(std::string(call) + " failed (XrResult " + std::to_string(result) + ")");
}
int main() {
    try {
        uint32_t count = 0;
        check(xrEnumerateInstanceExtensionProperties(nullptr, 0, &count, nullptr), "Enumerate extensions");
        std::vector<XrExtensionProperties> extensions(count, {XR_TYPE_EXTENSION_PROPERTIES});
        check(xrEnumerateInstanceExtensionProperties(nullptr, count, &count, extensions.data()), "Read extensions");
        std::vector<const char*> enabled;
#ifdef _WIN32
        for (const auto& extension : extensions)
            if (std::strcmp(extension.extensionName, XR_KHR_D3D11_ENABLE_EXTENSION_NAME) == 0)
                enabled.push_back(XR_KHR_D3D11_ENABLE_EXTENSION_NAME);
        if (enabled.empty()) throw std::runtime_error("Active runtime does not expose XR_KHR_D3D11_enable");
#endif
        XrInstanceCreateInfo create{XR_TYPE_INSTANCE_CREATE_INFO};
        std::strcpy(create.applicationInfo.applicationName, "Beetle VB OpenXR probe");
        create.applicationInfo.applicationVersion = 1;
        create.applicationInfo.apiVersion = XR_API_VERSION_1_0;
        create.enabledExtensionCount = static_cast<uint32_t>(enabled.size());
        create.enabledExtensionNames = enabled.data();
        XrInstance instance = XR_NULL_HANDLE;
        check(xrCreateInstance(&create, &instance), "Create instance");
        struct Cleanup { XrInstance instance; ~Cleanup() { xrDestroyInstance(instance); } } cleanup{instance};
        XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};
        check(xrGetInstanceProperties(instance, &properties), "Runtime properties");
        std::cout << "Runtime: " << properties.runtimeName << '\n';
        XrSystemGetInfo system_info{XR_TYPE_SYSTEM_GET_INFO};
        system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        XrSystemId system = XR_NULL_SYSTEM_ID;
        check(xrGetSystem(instance, &system_info, &system), "Find headset (connect it and start your OpenXR runtime)");
        XrSystemProperties system_properties{XR_TYPE_SYSTEM_PROPERTIES};
        check(xrGetSystemProperties(instance, system, &system_properties), "Headset properties");
        std::cout << "Headset: " << system_properties.systemName << '\n';
#ifdef _WIN32
        PFN_xrVoidFunction function = nullptr;
        check(xrGetInstanceProcAddr(instance, "xrGetD3D11GraphicsRequirementsKHR", &function), "D3D11 requirements function");
        XrGraphicsRequirementsD3D11KHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
        check(reinterpret_cast<PFN_xrGetD3D11GraphicsRequirementsKHR>(function)(instance, system, &requirements), "D3D11 requirements");
        std::cout << "D3D11 minimum feature level: " << requirements.minFeatureLevel
                  << " | adapter LUID: " << requirements.adapterLuid.HighPart << ':' << requirements.adapterLuid.LowPart << '\n';
#endif
        std::cout << "OpenXR diagnostic passed. No session or rendering started.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\nEnsure an OpenXR runtime is installed and selected as active.\n";
        return 1;
    }
}
