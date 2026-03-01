// Blur (adapted from Burn My Windows)
// SPDX-FileCopyrightText: Justin Garza JGarza9788@gmail.com
// SPDX-License-Identifier: GPL-3.0-or-later

#version 300 es
precision highp float;
in vec2 v_texcoord;

uniform float progress;
uniform vec2 resolution;
uniform sampler2D tex;
uniform float radius;

layout(location = 0) out vec4 fragColor;

const float BLUR_AMOUNT  = 5.0;
const float BLUR_QUALITY = 4.0;

float easeInOutSine(float t) {
    return -0.5 * (cos(3.141592653589793 * t) - 1.0);
}

float easeInOutCubic(float t) {
    return t < 0.5 ? 4.0 * t * t * t : (t - 1.0) * (2.0 * t - 2.0) * (2.0 * t - 2.0) + 1.0;
}

vec4 getBlurredInput(vec2 uv, float blurRadius, float samples) {
    vec4 color = vec4(0.0);
    const float tau = 6.28318530718;
    const float directions = 15.0;

    for (float d = 0.0; d < tau; d += tau / directions) {
        for (float s = 0.0; s < 1.0; s += 1.0 / samples) {
            vec2 offset = vec2(cos(d), sin(d)) * blurRadius * (1.0 - s) / resolution;
            vec2 sampleUV = uv + offset;
            if (sampleUV.x >= 0.0 && sampleUV.x <= 1.0 && sampleUV.y >= 0.0 && sampleUV.y <= 1.0)
                color += texture(tex, sampleUV);
        }
    }

    return color / samples / directions;
}

float roundedBoxSDF(vec2 center, vec2 halfSize, float r) {
    vec2 d = abs(center) - halfSize + r;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - r;
}

void main() {
    // closing: progress goes 0→1, we want blur to increase and alpha to decrease
    float p = 1.0 - progress;

    float easedBlur  = easeInOutSine(p);
    float easedAlpha = easeInOutCubic(p);

    float blurAmount = mix(BLUR_AMOUNT, 0.0, easedBlur);

    vec4 texColor = getBlurredInput(v_texcoord, blurAmount, BLUR_QUALITY);
    texColor.a = 1.0; // treat window as opaque (transparent terminals look wrong otherwise)

    // rounded corners
    if (radius > 0.0) {
        vec2 pixelPos = v_texcoord * resolution;
        float dist = roundedBoxSDF(pixelPos - resolution * 0.5, resolution * 0.5, radius);
        texColor.a *= 1.0 - smoothstep(-1.0, 1.0, dist);
    }

    texColor.a *= easedAlpha;

    fragColor = vec4(texColor.rgb * texColor.a, texColor.a);
}
