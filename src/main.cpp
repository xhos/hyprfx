#define WLR_USE_UNSTABLE

#include <hyprland/src/Compositor.hpp>

#include "globals.hpp"

static void onCloseWindow(void* self, SCallbackInfo& info, std::any data) {
    auto window = std::any_cast<PHLWINDOW>(data);

    // just in case hyprland explodes
    if (!window)
        return;

    HyprlandAPI::addNotification(PHANDLE, "[hyprfx] window closed", CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprfx] version mismatch (headers != running hyprland)", CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hyprfx] version mismatch");
    }

    static auto P = HyprlandAPI::registerCallbackDynamic(PHANDLE, "closeWindow", onCloseWindow);

    HyprlandAPI::addNotification(PHANDLE, "[hyprfx] init", CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);

    return {"hyprfx", "sick shader animations or smthn", "xhos", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {}
