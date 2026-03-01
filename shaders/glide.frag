// Glide (adapted from Burn My Windows)
// SPDX-FileCopyrightText: Simon Schneegans <code@simonschneegans.de>
// SPDX-License-Identifier: GPL-3.0-or-later

#version 300 es
precision highp float;
in vec2 v_texcoord;

uniform float progress;
uniform vec2 resolution;
uniform sampler2D tex;
uniform float radius;

layout(location = 0) out vec4 fragColor;

// tuning knobs
const float SCALE  = 0.5;
const float SQUISH = 0.5;
const float TILT   = -0.5;
const float SHIFT  = -0.4;

float easeOutQuad(float x) {
    return -1.0 * x * (x - 2.0);
}

float roundedBoxSDF(vec2 center, vec2 halfSize, float r) {
    vec2 d = abs(center) - halfSize + r;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - r;
}

void main() {
    float p = easeOutQuad(progress);

    // put texture coordinate origin to center of window
    vec2 coords = v_texcoord * 2.0 - 1.0;

    // scale image with progress
    coords /= mix(1.0, SCALE, p);

    // squish vertically
    coords.y /= mix(1.0, 1.0 - 0.2 * SQUISH, p);

    // tilt around x-axis
    coords.x /= mix(1.0, 1.0 - 0.1 * TILT * coords.y, p);

    // shift vertically
    coords.y += SHIFT * p;

    // back to 0..1
    coords = coords * 0.5 + 0.5;

    // sample window texture
    vec4 oColor = vec4(0.0);
    if (coords.x >= 0.0 && coords.x <= 1.0 && coords.y >= 0.0 && coords.y <= 1.0) {
        oColor = texture(tex, coords);
        oColor.a = 1.0;
    }

    // rounded corners
    if (radius > 0.0) {
        vec2 pixelPos = coords * resolution;
        float dist = roundedBoxSDF(pixelPos - resolution * 0.5, resolution * 0.5, radius);
        oColor.a *= 1.0 - smoothstep(-1.0, 1.0, dist);
    }

    // dissolve
    oColor.a *= pow(1.0 - progress, 2.0);

    fragColor = vec4(oColor.rgb * oColor.a, oColor.a);
}
