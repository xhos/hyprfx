#pragma once

#include <string>

inline const std::string VERT = R"#(
#version 300 es
precision highp float;
uniform mat3 proj;
uniform vec4 quad;
in vec2 pos;
out vec2 v_texcoord;

void main() {
    gl_Position = vec4(proj * vec3(pos, 1.0), 1.0);
    v_texcoord = (pos - quad.xy) / (quad.zw - quad.xy);
})#";

inline const std::string FRAG = R"#(
#version 300 es
precision highp float;
in vec2 v_texcoord;

uniform float progress;
uniform vec2 resolution;

layout(location = 0) out vec4 fragColor;

#define CELL_SIZE 16.0
#define PI 3.14159265

float hash(vec2 p) {
    uvec2 q = uvec2(ivec2(p)) * uvec2(1597334673u, 3812015801u);
    uint n = (q.x ^ q.y) * 1597334673u;
    return float(n) * (1.0 / float(0xffffffffu));
}

float hash2(vec2 p) {
    return hash(p + 100.0);
}

float hash3(vec2 p) {
    return hash(p + 200.0);
}

float drawSymbol(vec2 uv, float size, float symbol) {
    if (symbol < 0.33) {
        // circle
        return 1.0 - smoothstep(size - 0.03, size + 0.03, length(uv));
    } else if (symbol < 0.66) {
        // cross
        float d = min(abs(uv.x), abs(uv.y));
        return 1.0 - smoothstep(size * 0.35 - 0.03, size * 0.35 + 0.03, d);
    } else {
        // square
        float d = max(abs(uv.x), abs(uv.y));
        return 1.0 - smoothstep(size - 0.03, size + 0.03, d);
    }
}

void main() {
    vec2 fragCoord = v_texcoord * resolution;

    // placeholder color — will be window texture later
    vec3 col = vec3(0.3, 0.4, 0.6);
    col += 0.2 * smoothstep(0.4, 0.0, length(v_texcoord - vec2(0.5)));
    float luma = dot(col, vec3(0.299, 0.587, 0.114));

    vec2 cellId = floor(fragCoord / CELL_SIZE);
    vec2 cellUv = fract(fragCoord / CELL_SIZE) - 0.5;

    float rnd  = hash(cellId);
    float rnd2 = hash2(cellId);
    float rnd3 = hash3(cellId);

    // per-cell random dissolve
    float dissolved = smoothstep(rnd * 0.7, rnd * 0.7 + 0.3, progress);

    // symbol type and size
    float symbolSelect = fract(rnd2 + luma);
    float symbolSize = mix(0.15, 0.45, luma) * (1.0 - dissolved);

    // rotation during dissolve
    float angle = dissolved * rnd3 * PI;
    mat2 rot = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));

    // gravity drift
    vec2 drift = vec2(
        sin(rnd * PI * 2.0 + progress * 3.0) * 0.8,
        -1.0 - rnd * 3.0
    ) * dissolved * dissolved;

    float shape = drawSymbol(rot * cellUv + drift, symbolSize, symbolSelect);
    float alpha = shape * (1.0 - dissolved);

    fragColor = vec4(col * alpha, alpha);
})#";
