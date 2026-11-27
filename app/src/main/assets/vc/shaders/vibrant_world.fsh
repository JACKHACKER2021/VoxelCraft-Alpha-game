#version 300 es
precision mediump float;
uniform sampler2D u_atlas;
uniform vec3 u_fogColor;
uniform vec2 u_fogRange;
uniform float u_alpha;
uniform float u_brightness;
uniform float u_time;
uniform vec2 u_screen;
uniform float u_saturation;
uniform float u_contrast;
uniform float u_warmth;
uniform float u_vignette;
uniform float u_glow;
uniform vec3 u_shade;
uniform float u_fogStrength;
uniform highp sampler2DShadow u_shadowMap;
uniform float u_shadowStrength;
uniform vec4 u_handLight;
in vec3 v_world;
in vec2 v_uv;
in float v_light;
in float v_dist;
in float v_block;
in vec4 v_shadow;
in float v_kind;
out vec4 o_color;
// 1.5.5: how much sun reaches this pixel (1 lit, 0 in the shadow of a block), soft-edged.
float sunVisible() {
    if (u_shadowStrength <= 0.0) return 1.0;
    vec3 sp = v_shadow.xyz / v_shadow.w * 0.5 + 0.5;
    float edge = max(abs(sp.x - 0.5), abs(sp.y - 0.5)) * 2.0;
    if (edge >= 1.0 || sp.z >= 1.0) return 1.0;
    const float t = 1.0 / 1024.0;
    float z = sp.z - 0.0012;
    float s = texture(u_shadowMap, vec3(sp.xy + vec2(-0.6, -0.6) * t, z)) + texture(u_shadowMap, vec3(sp.xy + vec2(0.6, -0.6) * t, z))
            + texture(u_shadowMap, vec3(sp.xy + vec2(-0.6, 0.6) * t, z)) + texture(u_shadowMap, vec3(sp.xy + vec2(0.6, 0.6) * t, z));
    return mix(s * 0.25, 1.0, smoothstep(0.8, 1.0, edge));
}
void main() {
    vec4 c = texture(u_atlas, v_uv);
    if (u_alpha >= 1.0 && c.a < 0.5) discard;
    // Shadows only matter where the sky reaches: caves are dark already.
    // Leaves let some light through: they are shaded less by the leaves above them.
    float leaf = v_kind > 0.5 && v_kind < 1.5 ? 0.45 : 1.0;
    float sunVis = mix(1.0, sunVisible(), u_shadowStrength * leaf * smoothstep(0.35, 0.75, v_light));
    float sun = v_light * u_brightness * (0.58 + 0.42 * sunVis);
    // Out of the sun the light turns cool; in full sunlight it warms up.
    vec3 sky = vec3(sun) * mix(u_shade, vec3(1.0), smoothstep(0.25, 0.9, v_light) * sunVis);
    sky *= mix(vec3(1.0), vec3(1.0 + u_warmth, 1.0 + u_warmth * 0.35, 1.0 - u_warmth * 0.45), smoothstep(0.7, 1.0, v_light));
    // Lamps, lava and torches: a warm light that softly flickers.
    float flicker = 1.0 + 0.04 * sin(u_time * 7.0 + v_dist * 0.7);
    float hand = u_handLight.w * pow(clamp(1.0 - distance(v_world, u_handLight.xyz) / 11.0, 0.0, 1.0), 1.4);
    float block = max(v_block, hand);
    vec3 lamp = vec3(block, block * 0.86, block * 0.62) * flicker;
    vec3 col = c.rgb * max(sky, lamp) + c.rgb * lamp * u_glow * (1.0 - v_light);
    float g = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(g), col, u_saturation);
    col = (col - 0.5) * u_contrast + 0.5;
    col = clamp(col, 0.0, 1.0);
    float fog = clamp((v_dist - u_fogRange.x) / (u_fogRange.y - u_fogRange.x) * u_fogStrength, 0.0, 1.0);
    fog = fog * fog * (3.0 - 2.0 * fog);
    col = mix(col, u_fogColor, fog);
    vec2 q = gl_FragCoord.xy / u_screen - 0.5;
    col *= 1.0 - u_vignette * dot(q, q) * 1.6;
    o_color = vec4(col, c.a * u_alpha);
}