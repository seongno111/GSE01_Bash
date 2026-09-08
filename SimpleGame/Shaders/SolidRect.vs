#version 330 core
layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in float a_Intensity;
uniform vec2 u_Viewport;
uniform bool u_LinearScene;
out vec4 v_Color;
vec3 sRGBToLinear(vec3 x) {
    vec3 low = x/12.92;
    vec3 high = pow(max((x+0.055)/1.055, vec3(0)), vec3(2.4));
    return mix(low, high, step(vec3(0.04045), x));
}
void main() {
    vec2 p = a_Position / u_Viewport * 2.0 - 1.0;
    gl_Position = vec4(p.x, -p.y, 0.0, 1.0);
    v_Color = a_Color;
    if (u_LinearScene) v_Color.rgb = sRGBToLinear(a_Color.rgb)*a_Intensity;
}
