#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/Shader.hpp>

inline HANDLE PHANDLE = nullptr;

struct SGlobalState {
    SShader          shader;
    wl_event_source* tick = nullptr;
};

inline UP<SGlobalState> g_pGlobalState;
