#version 300 es
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in float a_light;
layout(location = 3) in float a_block;
uniform mat4 u_mvp;
uniform mat4 u_lightMvp;
uniform vec3 u_cam;
uniform float u_wave;
uniform float u_waveTime;
out vec2 v_uv;
out float v_light;
out float v_dist;
out float v_block;
out vec4 v_shadow;
out float v_kind;
out vec3 v_world;
void main() {
    // 1.5.5: the mesher marks swaying vertices (leaves, plant tops) and water surfaces in a_block.
    float kind = floor(a_block * 0.5);
    vec3 p = a_pos;
    if (u_wave > 0.0 && kind > 0.5) {
        float ph = u_waveTime * 1.7 + p.x * 0.55 + p.z * 0.45 + p.y * 0.3;
        if (kind < 1.5) {
            p.x += sin(ph) * 0.045 * u_wave;
            p.z += cos(ph * 0.8 + 1.3) * 0.04 * u_wave;
        } else {
            p.y += (sin(ph * 0.9) * 0.5 - 0.5) * 0.06 * u_wave;
        }
    }
    gl_Position = u_mvp * vec4(p, 1.0);
    v_uv = a_uv;
    v_light = a_light;
    v_block = a_block - 2.0 * kind;
    v_dist = length(p.xz - u_cam.xz);
    v_shadow = u_lightMvp * vec4(p, 1.0);
    v_kind = kind;
    v_world = p;
}