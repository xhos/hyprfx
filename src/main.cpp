#define WLR_USE_UNSTABLE

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/SharedDefs.hpp>
#include "globals.hpp"
#include "shaders.hpp"
#include "FxPassElement.hpp"
#include <hyprland/src/protocols/XDGShell.hpp>
#include <hyprland/src/xwayland/XSurface.hpp>

// compile-time shader selection
#define SHADER_BROKEN_GLASS 1
#define SHADER_AURA_GLOW    0
#define ACTIVE_SHADER       SHADER_AURA_GLOW

// actor scale for effects that render beyond window bounds
#if ACTIVE_SHADER == SHADER_BROKEN_GLASS
static constexpr float ACTOR_SCALE = 2.0f;
#else
static constexpr float ACTOR_SCALE = 1.0f;
#endif

struct SWindowAnimation {
    CBox                                  box;
    std::chrono::steady_clock::time_point startTime;
    float                                 duration    = 2.0f;
    GLuint                                snapshotTex = 0;
    int                                   texWidth    = 0;
    int                                   texHeight   = 0;
    float                                 rounding    = 0.f;
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

#if ACTIVE_SHADER == SHADER_BROKEN_GLASS
    GLuint prog = createProgram(VERT, FRAG_BROKEN_GLASS);
#else
    GLuint prog = createProgram(VERT, FRAG);
#endif

    g_pGlobalState->shader.program                             = prog;
    g_pGlobalState->shader.uniformLocations[SHADER_PROJ]       = glGetUniformLocation(prog, "proj");
    g_pGlobalState->shader.uniformLocations[SHADER_POS_ATTRIB] = glGetAttribLocation(prog, "pos");
    g_pGlobalState->shader.uniformLocations[SHADER_GRADIENT]   = glGetUniformLocation(prog, "quad");
    g_pGlobalState->shader.uniformLocations[SHADER_TIME]       = glGetUniformLocation(prog, "progress");
    g_pGlobalState->shader.uniformLocations[SHADER_FULL_SIZE]  = glGetUniformLocation(prog, "resolution");
    g_pGlobalState->shader.uniformLocations[SHADER_TEX]        = glGetUniformLocation(prog, "tex");
    g_pGlobalState->shader.uniformLocations[SHADER_RADIUS]     = glGetUniformLocation(prog, "radius");
}

void initShardTexture() {
    g_pHyprRenderer->makeEGLCurrent();

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 512, 512, 0, GL_RGB, GL_UNSIGNED_BYTE, SHARD_TEX_DATA);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);

    g_pGlobalState->shardTex = tex;
}

static GLuint blitTextureToOwn(GLuint srcTexID, GLenum srcTarget, int w, int h) {
    GLuint snapTex;
    glGenTextures(1, &snapTex);
    glBindTexture(GL_TEXTURE_2D, snapTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLint prevReadFB, prevDrawFB;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFB);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFB);

    GLuint readFbo, writeFbo;
    glGenFramebuffers(1, &readFbo);
    glGenFramebuffers(1, &writeFbo);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, srcTarget, srcTexID, 0);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, writeFbo);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, snapTex, 0);

    glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, prevReadFB);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDrawFB);
    glDeleteFramebuffers(1, &readFbo);
    glDeleteFramebuffers(1, &writeFbo);

    return snapTex;
}

static SP<CTexture> getSurfaceTexture(PHLWINDOW window) {
    if (!window->m_xdgSurface.expired()) {
        auto xdg = window->m_xdgSurface.lock();
        if (xdg && !xdg->m_surface.expired()) {
            auto surf = xdg->m_surface.lock();
            if (surf && surf->m_current.texture)
                return surf->m_current.texture;
        }
    }
    if (!window->m_xwaylandSurface.expired()) {
        auto xsurf = window->m_xwaylandSurface.lock();
        if (xsurf && !xsurf->m_surface.expired()) {
            auto surf = xsurf->m_surface.lock();
            if (surf && surf->m_current.texture)
                return surf->m_current.texture;
        }
    }
    return nullptr;
}

static void onCloseWindow(void* self, SCallbackInfo& info, std::any data) {
    auto window = std::any_cast<PHLWINDOW>(data);
    if (!window)
        return;

    g_pHyprRenderer->makeEGLCurrent();

    GLuint snapTex = 0;
    int    tw = 0, th = 0;

    // capture surface texture directly while it's still valid
    auto surfTex = getSurfaceTexture(window);
    if (surfTex && surfTex->m_texID > 0 && surfTex->m_size.x > 0) {
        tw      = (int)surfTex->m_size.x;
        th      = (int)surfTex->m_size.y;
        snapTex = blitTextureToOwn(surfTex->m_texID, surfTex->m_target, tw, th);
    }

    auto pos  = window->m_position;
    auto size = window->m_size;

    g_animations.push_back(SWindowAnimation{
        .box         = CBox{pos.x, pos.y, size.x, size.y},
        .startTime   = std::chrono::steady_clock::now(),
        .snapshotTex = snapTex,
        .texWidth    = tw,
        .texHeight   = th,
        .rounding    = window->rounding(),
    });
}

static void clearAnimations() {
    for (auto& anim : g_animations) {
        if (anim.snapshotTex)
            glDeleteTextures(1, &anim.snapshotTex);
    }
    g_animations.clear();
}

static void onWorkspace(void* self, SCallbackInfo& info, std::any data) {
    clearAnimations();
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
             .box         = anim.box,
             .progress    = progress,
             .snapshotTex = anim.snapshotTex,
             .rounding    = anim.rounding,
             .shardTex    = g_pGlobalState->shardTex,
             .actorScale  = ACTOR_SCALE,
        };

        g_pHyprRenderer->m_renderPass.add(makeUnique<CFxPassElement>(fxData));
    }
}

static int onTick(void* data) {
    auto now = std::chrono::steady_clock::now();

    for (auto& anim : g_animations) {
        // expand damage by actor scale so flying shards get redrawn
        float extraW = anim.box.width * (ACTOR_SCALE - 1.0f) * 0.5f;
        float extraH = anim.box.height * (ACTOR_SCALE - 1.0f) * 0.5f;
        CBox  dmg    = {anim.box.x - extraW - 2, anim.box.y - extraH - 2, anim.box.width * ACTOR_SCALE + 4, anim.box.height * ACTOR_SCALE + 4};
        g_pHyprRenderer->damageBox(dmg);
    }

    std::erase_if(g_animations, [&](const SWindowAnimation& anim) {
        float elapsed = std::chrono::duration<float>(now - anim.startTime).count();
        if (elapsed >= anim.duration) {
            if (anim.snapshotTex)
                glDeleteTextures(1, &anim.snapshotTex);
            return true;
        }
        return false;
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
    static auto P3 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "workspace", onWorkspace);

    g_pGlobalState = makeUnique<SGlobalState>();
    initShaders();
    initShardTexture();
    g_pGlobalState->tick = wl_event_loop_add_timer(g_pCompositor->m_wlEventLoop, &onTick, nullptr);
    wl_event_source_timer_update(g_pGlobalState->tick, 1);

    HyprlandAPI::addNotification(PHANDLE, "[hyprfx] init", CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);

    return {"hyprfx", "sick shader animations or smthn", "xhos", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    wl_event_source_remove(g_pGlobalState->tick);
    g_pHyprRenderer->m_renderPass.removeAllOfType("CFxPassElement");
    clearAnimations();
    if (g_pGlobalState->shardTex)
        glDeleteTextures(1, &g_pGlobalState->shardTex);
    g_pGlobalState->shader.destroy();
}
