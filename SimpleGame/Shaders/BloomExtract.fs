#version 330 core
in vec2 v_UV;
out vec4 FragColor;
uniform sampler2D u_Source;
uniform float u_Threshold;
uniform float u_Knee;
vec3 extractLight(vec3 color) {
    if (u_Threshold <= 0.0) return color;
    float brightness = max(color.r, max(color.g, color.b));
    float knee = max(u_Knee, 0.0001);
    float soft = clamp(brightness - u_Threshold + knee, 0.0, 2.0*knee);
    soft = soft*soft / (4.0*knee);
    return color * max(brightness-u_Threshold, soft) / max(brightness, 0.0001);
}
void main() {
    vec2 offset = 0.5 / vec2(textureSize(u_Source, 0));
    vec3 color = extractLight(texture(u_Source, v_UV+vec2(-offset.x,-offset.y)).rgb);
    color += extractLight(texture(u_Source, v_UV+vec2(offset.x,-offset.y)).rgb);
    color += extractLight(texture(u_Source, v_UV+vec2(-offset.x,offset.y)).rgb);
    color += extractLight(texture(u_Source, v_UV+offset).rgb);
    FragColor = vec4(color*0.25, 1.0);
}
