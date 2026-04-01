#include "ShaderLoader.hpp"

#include <hyprland/src/render/Renderer.hpp>
#include <fstream>

static const std::string VERT = R"(
#version 300 es
precision highp float;
uniform mat3 proj;
uniform vec4 quad;
in vec2 pos;
out vec2 v_texcoord;

void main() {
    gl_Position = vec4(proj * vec3(pos, 1.0), 1.0);
    v_texcoord = (pos - quad.xy) / (quad.zw - quad.xy);
}
)";

static const std::string FRAG_PRELUDE = R"(
#version 300 es
precision highp float;

in vec2 v_texcoord;

uniform sampler2D niri_tex;
uniform mat3 niri_geo_to_tex;
uniform float niri_progress;
uniform float niri_clamped_progress;
uniform float niri_random_seed;
uniform vec2 niri_size;

// niri shaders use texture2D (GLSL 100), alias it for ES 3.0
#define texture2D texture

layout(location = 0) out vec4 fragColor;

)";

static const std::string CLOSE_EPILOGUE = R"(
void main() {
    vec3 coords_geo = vec3(v_texcoord, 1.0);
    vec3 size_geo = vec3(1.0, 1.0, 1.0);
    vec4 color = close_color(coords_geo, size_geo);
    fragColor = color;
}
)";

static const std::string OPEN_EPILOGUE = R"(
void main() {
    vec3 coords_geo = vec3(v_texcoord, 1.0);
    vec3 size_geo = vec3(1.0, 1.0, 1.0);
    vec4 color = open_color(coords_geo, size_geo);
    fragColor = color;
}
)";

static GLuint compileShader(GLenum type, const std::string& src) {
    auto shader       = glCreateShader(type);
    auto shaderSource = src.c_str();

    glShaderSource(shader, 1, (const GLchar**)&shaderSource, nullptr);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_FALSE) {
        GLint logLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        std::string log(logLen, '\0');
        glGetShaderInfoLog(shader, logLen, nullptr, log.data());
        HyprlandAPI::addNotification(PHANDLE, "[hyprfx] shader compile error: " + log, CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static bool compileProgram(const std::string& vertSrc, const std::string& fragSrc, SCompiledShader& out) {
    auto vertShader = compileShader(GL_VERTEX_SHADER, vertSrc);
    if (!vertShader)
        return false;

    auto fragShader = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    if (!fragShader) {
        glDeleteShader(vertShader);
        return false;
    }

    auto prog = glCreateProgram();
    glAttachShader(prog, vertShader);
    glAttachShader(prog, fragShader);
    glLinkProgram(prog);

    glDetachShader(prog, vertShader);
    glDetachShader(prog, fragShader);
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (ok == GL_FALSE) {
        GLint logLen = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLen);
        std::string log(logLen, '\0');
        glGetProgramInfoLog(prog, logLen, nullptr, log.data());
        HyprlandAPI::addNotification(PHANDLE, "[hyprfx] shader link error: " + log, CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        glDeleteProgram(prog);
        return false;
    }

    out.program               = prog;
    out.proj                  = glGetUniformLocation(prog, "proj");
    out.posAttrib             = glGetAttribLocation(prog, "pos");
    out.quad                  = glGetUniformLocation(prog, "quad");
    out.niri_tex              = glGetUniformLocation(prog, "niri_tex");
    out.niri_geo_to_tex       = glGetUniformLocation(prog, "niri_geo_to_tex");
    out.niri_progress         = glGetUniformLocation(prog, "niri_progress");
    out.niri_clamped_progress = glGetUniformLocation(prog, "niri_clamped_progress");
    out.niri_random_seed      = glGetUniformLocation(prog, "niri_random_seed");
    out.niri_size             = glGetUniformLocation(prog, "niri_size");

    return true;
}

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprfx] could not open shader file: " + path, CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        return "";
    }
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

static bool loadShader(const std::string& path, eShaderType type, SCompiledShader& out) {
    auto userCode = readFile(path);
    if (userCode.empty())
        return false;

    const auto& epilogue = (type == SHADER_CLOSE) ? CLOSE_EPILOGUE : OPEN_EPILOGUE;
    std::string fragSrc  = FRAG_PRELUDE + userCode + epilogue;

    return compileProgram(VERT, fragSrc, out);
}

static std::string g_lastClosePath;
static std::string g_lastOpenPath;

void ensureShaderLoaded(eShaderType type) {
    static auto* const PCLOSE_SHADER = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprfx:close_shader")->getDataStaticPtr();
    static auto* const POPEN_SHADER  = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprfx:open_shader")->getDataStaticPtr();

    g_pHyprRenderer->makeEGLCurrent();

    if (type == SHADER_CLOSE) {
        std::string path(*PCLOSE_SHADER);
        if (path.empty() || (g_pGlobalState->closeShader.program && path == g_lastClosePath))
            return;
        g_pGlobalState->closeShader.destroy();
        if (loadShader(path, SHADER_CLOSE, g_pGlobalState->closeShader)) {
            g_lastClosePath = path;
        } else {
            HyprlandAPI::addNotification(PHANDLE, "[hyprfx] failed to load close shader: " + path, CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        }
    } else {
        std::string path(*POPEN_SHADER);
        if (path.empty() || (g_pGlobalState->openShader.program && path == g_lastOpenPath))
            return;
        g_pGlobalState->openShader.destroy();
        if (loadShader(path, SHADER_OPEN, g_pGlobalState->openShader)) {
            g_lastOpenPath = path;
        } else {
            HyprlandAPI::addNotification(PHANDLE, "[hyprfx] failed to load open shader: " + path, CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        }
    }
}

void reloadShaders() {
    g_lastClosePath.clear();
    g_lastOpenPath.clear();
    g_pGlobalState->closeShader.destroy();
    g_pGlobalState->openShader.destroy();
    ensureShaderLoaded(SHADER_CLOSE);
    ensureShaderLoaded(SHADER_OPEN);
}
