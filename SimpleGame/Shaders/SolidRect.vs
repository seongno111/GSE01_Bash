#version 330 core
// One instance per triangle: atlas first vertex and logical object index.
layout(location = 0) in uvec2 a_Triangle;
uniform sampler2D u_Geometry;
uniform samplerBuffer u_Instances;
uniform vec2 u_Viewport;
uniform bool u_LinearScene;
out vec4 v_Color;
out vec2 v_SpriteUV;
out vec2 v_LocalUV;
flat out int v_SpriteCode;
flat out float v_Radiance;
flat out vec4 v_EmissionBounds;

vec4 geometry(int texel)
{
    int width = textureSize(u_Geometry, 0).x;
    return texelFetch(u_Geometry, ivec2(texel % width, texel / width), 0);
}

vec3 sRGBToLinear(vec3 x)
{
    vec3 low = x / 12.92;
    vec3 high = pow(max((x + 0.055) / 1.055, vec3(0)), vec3(2.4));
    return mix(low, high, step(vec3(0.04045), x));
}

void main()
{
    int vertex = (int(a_Triangle.x) + gl_VertexID) * 2;
    vec4 shape = geometry(vertex);     // xy, red, green
    vec4 color = geometry(vertex + 1); // blue, alpha, radiance, padding
    int object = int(a_Triangle.y) * 5;
    vec4 axes = texelFetch(u_Instances, object);
    vec4 placement = texelFetch(u_Instances, object + 1);
    vec4 tint = texelFetch(u_Instances, object + 2);
    vec4 uv = texelFetch(u_Instances, object + 3);
    v_EmissionBounds = texelFetch(u_Instances, object + 4);
    vec2 screenPosition = shape.x * axes.xy + shape.y * axes.zw + placement.xy;
    vec2 p = screenPosition / u_Viewport * 2.0 - 1.0;
    gl_Position = vec4(p.x, -p.y, 0.0, 1.0);
    v_Color = vec4(shape.zw, color.xy) * tint;
    v_SpriteCode = int(placement.w + 0.5); // 0 = geometry, otherwise atlas index + 1.
    v_SpriteUV = mix(uv.xy, uv.zw, shape.xy);
    v_LocalUV = shape.xy;
    v_Radiance = color.z * placement.z;
    if (u_LinearScene && v_SpriteCode == 0)
    {
        v_Color.rgb = sRGBToLinear(v_Color.rgb) * v_Radiance;
    }
}
