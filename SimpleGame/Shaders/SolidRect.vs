#version 330 core
layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in float a_Intensity;
uniform vec2 u_Viewport;
uniform bool u_LinearScene;
uniform vec4 u_Model; // Translation XY, scale XY; works for cached local meshes.
uniform vec4 u_Tint;
uniform float u_Radiance;
out vec4 v_Color;

vec3 sRGBToLinear(vec3 x)
{
    vec3 low = x / 12.92;
    vec3 high = pow(max((x + 0.055) / 1.055, vec3(0)), vec3(2.4));
    return mix(low, high, step(vec3(0.04045), x));
}

void main()
{
    vec2 screenPosition = a_Position * u_Model.zw + u_Model.xy;
    vec2 p = screenPosition / u_Viewport * 2.0 - 1.0;
    gl_Position = vec4(p.x, -p.y, 0.0, 1.0);
    v_Color = a_Color * u_Tint;
    if (u_LinearScene)
    {
        v_Color.rgb = sRGBToLinear(v_Color.rgb) * a_Intensity * u_Radiance;
    }
}
