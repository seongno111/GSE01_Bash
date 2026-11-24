#version 330 core
layout(location = 0) in vec2 a_Center;
layout(location = 1) in vec2 a_XAxis;
layout(location = 2) in vec2 a_YAxis;
layout(location = 3) in vec4 a_Color;
layout(location = 4) in vec4 a_Shape;
uniform vec2 u_Viewport;
out vec2 v_Local;
flat out vec4 v_Color;
flat out vec4 v_Shape;
void main()
{
    const vec2 corners[4] = vec2[4](vec2(-1,-1), vec2(1,-1), vec2(-1,1), vec2(1,1));
    v_Local = corners[gl_VertexID];
    vec2 screen = a_Center + a_XAxis * v_Local.x + a_YAxis * v_Local.y;
    vec2 clip = screen / u_Viewport * 2.0 - 1.0;
    gl_Position = vec4(clip.x, -clip.y, 0, 1);
    v_Color = a_Color;
    v_Shape = a_Shape;
}
