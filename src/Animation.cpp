#define WLR_USE_UNSTABLE

#include "Animation.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/protocols/XDGShell.hpp>
#include <hyprland/src/xwayland/XSurface.hpp>

std::vector<SWindowAnimation> g_animations;

GLuint blitTextureToOwn(GLuint srcTexID, GLenum srcTarget, int w, int h) {
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

SP<CTexture> getSurfaceTexture(PHLWINDOW window) {
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

void clearAnimations() {
    for (auto& anim : g_animations) {
        if (anim.snapshotTex)
            glDeleteTextures(1, &anim.snapshotTex);
    }
    g_animations.clear();
}

int onTick(void* data) {
    auto now = std::chrono::steady_clock::now();

    if (!g_animations.empty()) {
        for (auto& m : g_pCompositor->m_monitors) {
            g_pHyprRenderer->damageMonitor(m);
        }
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
