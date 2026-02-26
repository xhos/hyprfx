#define WLR_USE_UNSTABLE

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/SharedDefs.hpp>

#include "globals.hpp"
#include "shaders.hpp"
#include "FxPassElement.hpp"

struct SWindowAnimation {
    CBox                                  box;
    std::chrono::steady_clock::time_point startTime;
    float                                 duration = 2.0f;
};

static std::vector<SWindowAnimation> g_animations;

GLuint                               compileShader(const GLuint& type, const std::string& src) {
    auto shader       = glCreateShader(type);
    auto shaderSource = src.c_str();

    glShaderSource(shader, 1, (const GLchar**)&shaderSource, nullptr);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_FALSE)
        throw std::runtime_error("[hyprfx] shader compilation failed");

    return shader;
}

GLuint createProgram(const std::string& vert, const std::string& frag) {
    auto vertCompiled = compileShader(GL_VERTEX_SHADER, vert);
    auto fragCompiled = compileShader(GL_FRAGMENT_SHADER, frag);

    auto prog = glCreateProgram();
    glAttachShader(prog, vertCompiled);
    glAttachShader(prog, fragCompiled);
    glLinkProgram(prog);

    glDetachShader(prog, vertCompiled);
    glDetachShader(prog, fragCompiled);
    glDeleteShader(vertCompiled);
    glDeleteShader(fragCompiled);

    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (ok == GL_FALSE)
        throw std::runtime_error("[hyprfx] shader linking failed");

    return prog;
}

void initShaders() {
    g_pHyprRenderer->makeEGLCurrent();

    GLuint prog = createProgram(VERT, FRAG);

    g_pGlobalState->shader.program                             = prog;
    g_pGlobalState->shader.uniformLocations[SHADER_PROJ]       = glGetUniformLocation(prog, "proj");
    g_pGlobalState->shader.uniformLocations[SHADER_POS_ATTRIB] = glGetAttribLocation(prog, "pos");
    g_pGlobalState->shader.uniformLocations[SHADER_GRADIENT]   = glGetUniformLocation(prog, "quad");
    g_pGlobalState->shader.uniformLocations[SHADER_TIME]       = glGetUniformLocation(prog, "progress");
    g_pGlobalState->shader.uniformLocations[SHADER_FULL_SIZE]  = glGetUniformLocation(prog, "resolution");
}

static void onCloseWindow(void* self, SCallbackInfo& info, std::any data) {
    auto window = std::any_cast<PHLWINDOW>(data);
    if (!window)
        return;

    auto pos  = window->m_realPosition->value();
    auto size = window->m_realSize->value();

    g_animations.push_back(SWindowAnimation{
        .box       = CBox{pos.x, pos.y, size.x, size.y},
        .startTime = std::chrono::steady_clock::now(),
    });
}

static void onRenderStage(void* self, SCallbackInfo& info, std::any data) {
    auto stage = std::any_cast<eRenderStage>(data);
    if (stage != RENDER_POST_WINDOWS)
        return;

    if (g_animations.empty())
        return;

    auto now = std::chrono::steady_clock::now();

    for (auto& anim : g_animations) {
        float elapsed  = std::chrono::duration<float>(now - anim.startTime).count();
        float progress = std::min(elapsed / anim.duration, 1.0f);

        auto  fxData = CFxPassElement::SFxData{
             .box      = anim.box,
             .progress = progress,
        };

        g_pHyprRenderer->m_renderPass.add(makeUnique<CFxPassElement>(fxData));
    }
}

static int onTick(void* data) {
    auto now = std::chrono::steady_clock::now();

    for (auto& anim : g_animations) {
        CBox dmg = {anim.box.x - 2, anim.box.y - 2, anim.box.width + 4, anim.box.height + 4};
        g_pHyprRenderer->damageBox(dmg);
    }

    std::erase_if(g_animations, [&](const SWindowAnimation& anim) {
        float elapsed = std::chrono::duration<float>(now - anim.startTime).count();
        return elapsed >= anim.duration;
    });

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
    initShaders();
    g_pGlobalState->tick = wl_event_loop_add_timer(g_pCompositor->m_wlEventLoop, &onTick, nullptr);
    wl_event_source_timer_update(g_pGlobalState->tick, 1);

    HyprlandAPI::addNotification(PHANDLE, "[hyprfx] init", CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);

    return {"hyprfx", "sick shader animations or smthn", "xhos", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    wl_event_source_remove(g_pGlobalState->tick);
    g_pHyprRenderer->m_renderPass.removeAllOfType("CFxPassElement");
    g_pGlobalState->shader.destroy();
}
