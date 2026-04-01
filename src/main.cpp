#define WLR_USE_UNSTABLE

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/SharedDefs.hpp>
#include "globals.hpp"
#include "ShaderLoader.hpp"
#include "Animation.hpp"
#include "FxPassElement.hpp"

#include <random>

static std::mt19937                        g_rng{std::random_device{}()};
static std::uniform_real_distribution<float> g_seedDist{0.f, 1.f};

static void onCloseWindow(void* self, SCallbackInfo& info, std::any data) {
    ensureShaderLoaded(SHADER_CLOSE);

    if (!g_pGlobalState->closeShader.program)
        return;

    auto window = std::any_cast<PHLWINDOW>(data);
    if (!window)
        return;

    g_pHyprRenderer->makeEGLCurrent();

    auto surfTex = getSurfaceTexture(window);
    if (!surfTex || surfTex->m_texID == 0 || surfTex->m_size.x <= 0)
        return;

    int    tw      = (int)surfTex->m_size.x;
    int    th      = (int)surfTex->m_size.y;
    GLuint snapTex = blitTextureToOwn(surfTex->m_texID, surfTex->m_target, tw, th);
    if (!snapTex)
        return;

    static auto* const PDURATION = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprfx:close_duration_ms")->getDataStaticPtr();

    auto pos  = window->m_position;
    auto size = window->m_size;

    g_animations.push_back(SWindowAnimation{
        .box         = CBox{pos.x, pos.y, size.x, size.y},
        .startTime   = std::chrono::steady_clock::now(),
        .duration    = (float)**PDURATION / 1000.f,
        .snapshotTex = snapTex,
        .texWidth    = tw,
        .texHeight   = th,
        .randomSeed  = g_seedDist(g_rng),
        .type        = SHADER_CLOSE,
    });
}

static void onWorkspace(void* self, SCallbackInfo& info, std::any data) {
    clearAnimations();
}

static void onRenderStage(void* self, SCallbackInfo& info, std::any data) {
    auto stage = std::any_cast<eRenderStage>(data);
    if (stage != RENDER_POST_WINDOWS || g_animations.empty())
        return;

    auto now = std::chrono::steady_clock::now();

    for (auto& anim : g_animations) {
        float elapsed         = std::chrono::duration<float>(now - anim.startTime).count();
        float progress        = elapsed / anim.duration;
        float clampedProgress = std::clamp(progress, 0.f, 1.f);

        SCompiledShader* shader = (anim.type == SHADER_CLOSE)
            ? &g_pGlobalState->closeShader
            : &g_pGlobalState->openShader;

        if (!shader->program)
            continue;

        g_pHyprRenderer->m_renderPass.add(makeUnique<CFxPassElement>(CFxPassElement::SFxData{
            .box             = anim.box,
            .progress        = progress,
            .clampedProgress = clampedProgress,
            .snapshotTex     = anim.snapshotTex,
            .randomSeed      = anim.randomSeed,
            .windowW         = (float)anim.texWidth,
            .windowH         = (float)anim.texHeight,
            .shader          = shader,
        }));
    }
}

static void onConfigReloaded(void* self, SCallbackInfo& info, std::any data) {
    reloadShaders();
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

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprfx:close_shader", Hyprlang::STRING{""});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprfx:open_shader", Hyprlang::STRING{""});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprfx:close_duration_ms", Hyprlang::INT{500});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprfx:open_duration_ms", Hyprlang::INT{400});

    static auto P1 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "closeWindow", onCloseWindow);
    static auto P2 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "render", onRenderStage);
    static auto P3 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "workspace", onWorkspace);
    static auto P4 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "configReloaded", onConfigReloaded);

    g_pGlobalState = makeUnique<SGlobalState>();
    reloadShaders();
    g_pGlobalState->tick = wl_event_loop_add_timer(g_pCompositor->m_wlEventLoop, &onTick, nullptr);
    wl_event_source_timer_update(g_pGlobalState->tick, 1);

    HyprlandAPI::addNotification(PHANDLE, "[hyprfx] loaded", CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);

    return {"hyprfx", "niri-compatible shader animations for Hyprland", "xhos", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    wl_event_source_remove(g_pGlobalState->tick);
    g_pHyprRenderer->m_renderPass.removeAllOfType("CFxPassElement");
    clearAnimations();
    g_pGlobalState->closeShader.destroy();
    g_pGlobalState->openShader.destroy();
}
