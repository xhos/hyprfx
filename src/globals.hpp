#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <GLES3/gl3.h>

inline HANDLE PHANDLE = nullptr;

enum eShaderType {
    SHADER_CLOSE,
    SHADER_OPEN,
};

struct SCompiledShader {
    GLuint program = 0;

    // niri-compatible uniform locations
    GLint proj                  = -1;
    GLint posAttrib             = -1;
    GLint quad                  = -1;
    GLint niri_tex              = -1;
    GLint niri_geo_to_tex       = -1;
    GLint niri_progress         = -1;
    GLint niri_clamped_progress = -1;
    GLint niri_random_seed      = -1;
    GLint niri_size             = -1;

    void  destroy() {
        if (program)
            glDeleteProgram(program);
        program = 0;
    }
};

struct SGlobalState {
    SCompiledShader  closeShader;
    SCompiledShader  openShader;
    wl_event_source* tick = nullptr;
};

inline UP<SGlobalState> g_pGlobalState;
