// Broken Glass (adapted from Burn My Windows GNOME extension)
// SPDX-FileCopyrightText: Simon Schneegans <code@simonschneegans.de>
// SPDX-License-Identifier: GPL-3.0-or-later

#version 300 es
precision highp float;
in vec2 v_texcoord;

uniform float progress;
uniform vec2 resolution;
uniform sampler2D tex;
uniform float radius;

uniform sampler2D shardTex;
uniform vec2 seed;
uniform vec2 epicenter;
uniform float shardScale;
uniform float blowForce;
uniform float gravity;

layout(location = 0) out vec4 fragColor;

const float SHARD_LAYERS = 5.0;
const float ACTOR_SCALE  = 2.0;
const float PADDING      = ACTOR_SCALE / 2.0 - 0.5;

vec4 getInputColor(vec2 coords) {
    // clamp to window bounds — outside is transparent
    if (coords.x < 0.0 || coords.x > 1.0 || coords.y < 0.0 || coords.y > 1.0)
        return vec4(0.0);
    vec4 color = texture(tex, coords);
    color.a = 1.0;
    return color;
}

float roundedBoxSDF(vec2 center, vec2 halfSize, float r) {
    vec2 d = abs(center) - halfSize + r;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - r;
}

void main() {
    vec4 oColor = vec4(0.0);

    float p = progress;

    for (float i = 0.0; i < SHARD_LAYERS; ++i) {
        // Scale texcoords to account for expanded 2x actor
        vec2 coords = v_texcoord * ACTOR_SCALE - PADDING;

        // Scale and rotate around epicenter
        coords -= epicenter;

        // Scale each layer a bit differently
        coords /= mix(1.0, 1.0 + blowForce * (i + 2.0) / SHARD_LAYERS, p);

        // Rotate each layer a bit differently
        float rotation = (mod(i, 2.0) - 0.5) * 0.2 * p;
        coords = vec2(coords.x * cos(rotation) - coords.y * sin(rotation),
                       coords.x * sin(rotation) + coords.y * cos(rotation));

        // Gravity: pull fragments downward (negative in sample space = downward on screen)
        float grav = gravity * 0.1 * (i + 1.0) * p * p;
        coords -= vec2(0.0, grav);

        // Restore correct position
        coords += epicenter;

        // Retrieve shard info from texture for this layer
        vec2 shardCoords = (coords + seed) * resolution / shardScale / 500.0;
        vec2 shardMap = texture(shardTex, shardCoords).rg;

        // Green channel: random value per shard, discretized into layer bins
        float shardGroup = floor(shardMap.g * SHARD_LAYERS * 0.999);

        if (shardGroup == i && (shardMap.x - pow(p + 0.1, 2.0)) > 0.0) {
            vec4 c = getInputColor(coords);

            // Apply rounded corners
            if (radius > 0.0) {
                vec2 pixelPos = coords * resolution;
                float dist = roundedBoxSDF(pixelPos - resolution * 0.5, resolution * 0.5, radius);
                c.a *= 1.0 - smoothstep(-1.0, 1.0, dist);
            }

            // Fade out gradually — each layer fades at a slightly different rate
            float layerOffset = i / SHARD_LAYERS * 0.3;
            float fade = 1.0 - smoothstep(0.3 + layerOffset, 0.8 + layerOffset * 0.5, p);
            c.a *= fade;

            oColor = c;
        }
    }

    fragColor = vec4(oColor.rgb * oColor.a, oColor.a);
}
