#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Color;
layout(location = 3) in vec4 a_Emission;
layout(location = 4) in uvec4 a_Joints0;
layout(location = 5) in vec4 a_Weights0;
layout(location = 6) in uvec4 a_Joints1;
layout(location = 7) in vec4 a_Weights1;
layout(location = 8) in vec3 a_Surface;
uniform samplerBuffer u_Pose;
uniform vec2 u_Viewport;
uniform vec3 u_Placement;   // screen feet XY, zoom/world scale
uniform vec4 u_Orientation; // model +X and +Z in world ground coordinates
out vec3 v_Position;
out vec3 v_Normal;
out vec4 v_Color;
out vec4 v_Emission;
flat out vec3 v_Surface;

mat4 bone(uint index)
{
    int offset = int(index) * 4;
    return mat4(texelFetch(u_Pose, offset),
                texelFetch(u_Pose, offset + 1),
                texelFetch(u_Pose, offset + 2),
                texelFetch(u_Pose, offset + 3));
}

void main()
{
    mat4 skin = mat4(0);
    for (int i = 0; i < 4; ++i)
    {
        if (a_Weights0[i] > 0.0)
        {
            skin += bone(a_Joints0[i]) * a_Weights0[i];
        }
        if (a_Weights1[i] > 0.0)
        {
            skin += bone(a_Joints1[i]) * a_Weights1[i];
        }
    }
    vec3 local = (skin * vec4(a_Position, 1)).xyz;
    vec2 ground = u_Orientation.xy * local.x + u_Orientation.zw * local.z;
    vec2 screen =
        u_Placement.xy
        + vec2((ground.x - ground.y) * 40.0, (ground.x + ground.y) * 20.0 - local.y * 40.0)
              * u_Placement.z;
    vec2 clip = screen / u_Viewport * 2.0 - 1.0;
    // Depth is local to one painter-ordered actor, matching the existing scene graph.
    gl_Position = vec4(clip.x, -clip.y, -(ground.x + ground.y + local.y) / 128.0, 1);
    v_Position = local;
    mat3 linear = mat3(skin);
    float determinantValue = determinant(linear);
    v_Normal = abs(determinantValue) > 0.000001 ? transpose(inverse(linear)) * a_Normal : a_Normal;
    v_Color = a_Color;
    v_Emission = a_Emission;
    v_Surface = a_Surface;
}
