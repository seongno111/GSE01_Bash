#version 330 core
in vec2 v_UV;
out vec4 FragColor;
uniform sampler2D u_Source;
uniform vec2 u_Direction;

void main()
{
    vec2 stepUV = u_Direction / vec2(textureSize(u_Source, 0));
    vec3 color = texture(u_Source, v_UV).rgb * 0.227027;
    color += texture(u_Source, v_UV + stepUV * 1.384615).rgb * 0.316216;
    color += texture(u_Source, v_UV - stepUV * 1.384615).rgb * 0.316216;
    color += texture(u_Source, v_UV + stepUV * 3.230769).rgb * 0.070270;
    color += texture(u_Source, v_UV - stepUV * 3.230769).rgb * 0.070270;
    FragColor = vec4(color, 1.0);
}
