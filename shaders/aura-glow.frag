// Aura Glow (adapted from Burn My Windows GNOME extension)
// Original: SPDX-FileCopyrightText: Justin Garza, Simon Schneegans
// Original: SPDX-License-Identifier: GPL-3.0-or-later

#version 300 es
precision highp float;
in vec2 v_texcoord;

uniform float progress;
uniform vec2 resolution;
uniform sampler2D tex;
uniform float radius;

layout(location = 0) out vec4 fragColor;

// --- tunables ---
#define SPEED        1.0
#define SATURATION   1.5
#define START_HUE    0.0
#define EDGE_SIZE    0.4
#define EDGE_HARD    0.5
#define SEED         vec2(3.17, 7.23)

// --- helpers from burn-my-windows common.glsl ---

float easeInSine(float t)   { return 1.0 - cos(t * 3.141592653589793 * 0.5); }
float easeOutSine(float t)  { return sin(t * 3.141592653589793 * 0.5); }
float easeOutCubic(float t) { float f = t - 1.0; return f * f * f + 1.0; }

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec2 hash22(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * vec3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.xx + p3.yz) * p3.zy);
}

float simplex2D(vec2 p) {
    const float K1 = 0.366025404;
    const float K2 = 0.211324865;
    vec2  i = floor(p + (p.x + p.y) * K1);
    vec2  a = p - i + (i.x + i.y) * K2;
    float m = step(a.y, a.x);
    vec2  o = vec2(m, 1.0 - m);
    vec2  b = a - o + K2;
    vec2  c = a - 1.0 + 2.0 * K2;
    vec3  h = max(0.5 - vec3(dot(a, a), dot(b, b), dot(c, c)), 0.0);
    vec3  n = h * h * h * h *
             vec3(dot(a, -1.0 + 2.0 * hash22(i + 0.0)),
                  dot(b, -1.0 + 2.0 * hash22(i + o)),
                  dot(c, -1.0 + 2.0 * hash22(i + 1.0)));
    return 0.5 + 0.5 * dot(n, vec3(70.0));
}

vec3 offsetHue(vec3 color, float hueOffset) {
    float maxC  = max(max(color.r, color.g), color.b);
    float minC  = min(min(color.r, color.g), color.b);
    float delta = maxC - minC;
    float hue   = 0.0;
    if (delta > 0.0) {
        if (maxC == color.r)      hue = mod((color.g - color.b) / delta, 6.0);
        else if (maxC == color.g) hue = (color.b - color.r) / delta + 2.0;
        else                      hue = (color.r - color.g) / delta + 4.0;
    }
    hue /= 6.0;
    float sat = (maxC > 0.0) ? (delta / maxC) : 0.0;
    float val = maxC;
    hue = mod(hue + hueOffset, 1.0);
    float cc = val * sat;
    float x  = cc * (1.0 - abs(mod(hue * 6.0, 2.0) - 1.0));
    float mm = val - cc;
    vec3 rgb;
    if      (hue < 1.0/6.0) rgb = vec3(cc, x,  0.0);
    else if (hue < 2.0/6.0) rgb = vec3(x,  cc, 0.0);
    else if (hue < 3.0/6.0) rgb = vec3(0.0, cc, x);
    else if (hue < 4.0/6.0) rgb = vec3(0.0, x,  cc);
    else if (hue < 5.0/6.0) rgb = vec3(x,  0.0, cc);
    else                     rgb = vec3(cc, 0.0, x);
    return rgb + mm;
}

vec4 alphaOver(vec4 under, vec4 over) {
    if (under.a == 0.0 && over.a == 0.0) return vec4(0.0);
    float a = mix(under.a, 1.0, over.a);
    return vec4(mix(under.rgb * under.a, over.rgb, over.a) / a, a);
}

float roundedBoxSDF(vec2 center, vec2 halfSize, float r) {
    vec2 d = abs(center) - halfSize + r;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - r;
}

// --- main effect ---

void main() {
    float p = 1.0 - progress;

    vec2 uv = v_texcoord;

    // 1:1 aspect ratio correction
    vec2 oneToOneUV = uv - 0.5;
    if (resolution.x > resolution.y)
        oneToOneUV.y *= resolution.y / resolution.x;
    else
        oneToOneUV.x *= resolution.x / resolution.y;
    oneToOneUV += 0.5;
    uv = mix(oneToOneUV, uv, p);

    // shape gradient: square edges -> circle
    float shape    = mix(2.0, 100.0, pow(p, 5.0));
    float gradient = pow(abs(uv.x - 0.5) * 2.0, shape)
                   + pow(abs(uv.y - 0.5) * 2.0, shape);
    gradient += simplex2D(v_texcoord + SEED) * 0.5;

    // glow mask
    float glowMask = (p - gradient) / (EDGE_SIZE + 0.1);
    glowMask = 1.0 - clamp(glowMask, 0.0, 1.0);
    glowMask *= easeOutSine(min(1.0, (1.0 - p) * 4.0));

    // sample window texture with slight scale-up
    vec2 windowUV = (v_texcoord - 0.5) * mix(1.1, 1.0, easeOutCubic(p)) + 0.5;
    vec4 windowColor = texture(tex, windowUV);
    windowColor.a = 1.0;

    // apply rounded corners
    if (radius > 0.0) {
        vec2 pixelPos = v_texcoord * resolution;
        float dist = roundedBoxSDF(pixelPos - resolution * 0.5, resolution * 0.5, radius);
        windowColor.a *= 1.0 - smoothstep(-1.0, 1.0, dist);
    }

    // animated rainbow glow
    vec3 glowColor = cos(p * SPEED + uv.xyx + vec3(0.0, 2.0, 4.0));
    glowColor = offsetHue(glowColor, START_HUE + 0.1);
    glowColor = clamp(glowColor * SATURATION, vec3(0.0), vec3(1.0));

    windowColor.rgb += glowColor * glowMask;
    windowColor = alphaOver(windowColor, vec4(glowColor, glowMask * 0.2));

    // crop mask: shrinks window into a circle
    float softCrop = 1.0 - smoothstep(p - 0.5, p + 0.5, gradient);
    float hardCrop = 1.0 - smoothstep(p - 0.05, p + 0.05, gradient);
    float cropMask = mix(softCrop, hardCrop, EDGE_HARD);
    cropMask *= easeInSine(min(1.0, p * 2.0));
    cropMask  = max(cropMask, 1.0 - easeOutSine(min(1.0, (1.0 - p) * 4.0)));

    windowColor.a *= cropMask;

    fragColor = vec4(windowColor.rgb * windowColor.a, windowColor.a);
}
