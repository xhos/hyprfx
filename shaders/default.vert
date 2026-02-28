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
