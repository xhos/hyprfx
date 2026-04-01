#pragma once

#include "globals.hpp"
#include <hyprland/src/render/Texture.hpp>
#include <chrono>
#include <vector>

struct SWindowAnimation {
    CBox                                  box;
    std::chrono::steady_clock::time_point startTime;
    float                                 duration    = 0.5f;
    GLuint                                snapshotTex = 0;
    int                                   texWidth    = 0;
    int                                   texHeight   = 0;
    float                                 randomSeed  = 0.f;
    eShaderType                           type        = SHADER_CLOSE;
};

extern std::vector<SWindowAnimation> g_animations;

GLuint blitTextureToOwn(GLuint srcTexID, GLenum srcTarget, int w, int h);
SP<CTexture> getSurfaceTexture(PHLWINDOW window);
void clearAnimations();
int  onTick(void* data);
