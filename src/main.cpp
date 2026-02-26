#define WLR_USE_UNSTABLE

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/SharedDefs.hpp>

#include "globals.hpp"
#include "FxPassElement.hpp"

struct SWindowAnimation {
    CBox                                  box;       // window position/size at time of close
    std::chrono::steady_clock::time_point startTime;
    float                                 duration = 1.0f; // seconds
};

static std::vector<SWindowAnimation> g_animations;

static void onCloseWindow(void* self, SCallbackInfo& info, std::any data) {
    auto window = std::any_cast<PHLWINDOW>(data);
    if (!window)
        return;

    auto pos  = window->m_realPosition->value();
    auto size = window->m_realSize->value();

    g_animations.push_back(SWindowAnimation{
        .box       = CBox{pos.x, pos.y, size.x, size.y},
        .startTime = std::chrono::steady_clock::now(),
        .duration  = 1.0f,
    });
}

static void onRenderStage(void* self, SCallbackInfo& info, std::any data) {
    auto stage = std::any_cast<eRenderStage>(data);
    if (stage != RENDER_POST_WINDOWS)
        return;

    if (g_animations.empty())
        return;

    auto monitor = g_pHyprOpenGL->m_renderData.pMonitor.lock();
    if (!monitor)
        return;

    auto now = std::chrono::steady_clock::now();

    for (auto& anim : g_animations) {
        float elapsed  = std::chrono::duration<float>(now - anim.startTime).count();
        float progress = std::min(elapsed / anim.duration, 1.0f);

        CBox monBox = {anim.box.x - monitor->m_position.x, anim.box.y - monitor->m_position.y, anim.box.width, anim.box.height};
        monBox.scale(monitor->m_scale).round();

        auto fxData = CFxPassElement::SFxData{
            .box   = monBox,
            .alpha = 1.0f - progress,
        };

        g_pHyprRenderer->m_renderPass.add(makeUnique<CFxPassElement>(fxData));
    }
}

static int onTick(void* data) {
    auto now = std::chrono::steady_clock::now();

    std::erase_if(g_animations, [&](const SWindowAnimation& anim) {
        float elapsed = std::chrono::duration<float>(now - anim.startTime).count();
        return elapsed >= anim.duration;
    });

    for (auto& anim : g_animations) {
        g_pHyprRenderer->damageBox(anim.box);
    }

    const int TIMEOUT = g_pHyprRenderer->m_mostHzMonitor ? 1000.0 / g_pHyprRenderer->m_mostHzMonitor->m_refreshRate : 16;
    wl_event_source_timer_update(g_pGlobalState->tick, TIMEOUT);

    return 0;
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

    static auto P1 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "closeWindow", onCloseWindow);
    static auto P2 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "render", onRenderStage);

    g_pGlobalState = makeUnique<SGlobalState>();
    g_pGlobalState->tick = wl_event_loop_add_timer(g_pCompositor->m_wlEventLoop, &onTick, nullptr);
    wl_event_source_timer_update(g_pGlobalState->tick, 1);

    HyprlandAPI::addNotification(PHANDLE, "[hyprfx] init", CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);

    return {"hyprfx", "sick shader animations or smthn", "xhos", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    wl_event_source_remove(g_pGlobalState->tick);
    g_pHyprRenderer->m_renderPass.removeAllOfType("CFxPassElement");
}
