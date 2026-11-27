#version 300 es
precision mediump float;
uniform sampler2D u_atlas;
uniform vec3 u_fogColor;
uniform vec2 u_fogRange;
uniform float u_alpha;
uniform float u_brightness;
uniform vec4 u_handLight;
in vec2 v_uv;
in float v_light;
in float v_dist;
in float v_block;
in vec3 v_world;
out vec4 o_color;
void main() {
    vec4 c = texture(u_atlas, v_uv);
    if (u_alpha >= 1.0 && c.a < 0.5) discard;
    float fog = clamp((v_dist - u_fogRange.x) / (u_fogRange.y - u_fogRange.x), 0.0, 1.0);
    // A torch in the hand lights the blocks around the player.
    float hand = u_handLight.w * pow(clamp(1.0 - distance(v_world, u_handLight.xyz) / 11.0, 0.0, 1.0), 1.4);
    float block = max(v_block, hand);
    // The sky's light dims at night; lamps and lava keep their warm light.
    vec3 lit = max(vec3(v_light * u_brightness), vec3(block, block * 0.9, block * 0.72));
    o_color = vec4(mix(min(c.rgb * lit, vec3(1.0)), u_fogColor, fog), c.a * u_alpha);
}