#pragma once

#include <string>

// simple vertex shader: transforms position via projection matrix, passes through texcoord
inline const std::string VERT = R"#(
#version 300 es
precision mediump float;
uniform mat3 proj;
in vec2 pos;
in vec2 texcoord;
out vec2 v_texcoord;

void main() {
    gl_Position = vec4(proj * vec3(pos, 1.0), 1.0);
    v_texcoord = texcoord;
})#";

// for now just render a solid color to prove the pass element works
inline const std::string FRAG = R"#(
#version 300 es
precision mediump float;
in vec2 v_texcoord;
uniform vec4 color;

layout(location = 0) out vec4 fragColor;

void main() {
    fragColor = color;
})#";
