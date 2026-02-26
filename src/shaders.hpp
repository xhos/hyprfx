#pragma once

#include <string>

inline const std::string VERT = R"#(
#version 300 es
precision mediump float;
uniform mat3 proj;
in vec2 pos;
out vec2 v_texcoord;

void main() {
    gl_Position = vec4(proj * vec3(pos, 1.0), 1.0);
})#";

inline const std::string FRAG = R"#(
#version 300 es
precision mediump float;
uniform vec4 color;

layout(location = 0) out vec4 fragColor;

void main() {
    fragColor = color;
})#";
