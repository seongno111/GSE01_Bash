#version 330 core
layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec4 a_Color;
uniform vec2 u_Viewport;
out vec4 v_Color;
void main() {
    vec2 p = a_Position / u_Viewport * 2.0 - 1.0;
    gl_Position = vec4(p.x, -p.y, 0.0, 1.0);
    v_Color = a_Color;
}
