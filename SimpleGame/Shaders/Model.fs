#version 330 core
in vec3 v_Position;
in vec3 v_Normal;
in vec4 v_Color;
in vec4 v_Emission;
flat in vec3 v_Surface; // alpha cutoff (-1 for opaque), roughness, metallic
uniform vec4 u_Tint;
uniform vec4 u_Orientation;
uniform bool u_LinearScene;
uniform vec4 u_LampPosition; // xyz, range (0 means no range cutoff)
uniform vec4 u_LampColor;    // linear RGB, photometric intensity
out vec4 o_Color;

vec3 linearToSrgb(vec3 value)
{
    return mix(value * 12.92,
               1.055 * pow(max(value, vec3(0)), vec3(1.0 / 2.4)) - 0.055,
               step(vec3(0.0031308), value));
}

void main()
{
    if (v_Color.a < max(v_Surface.x, 0.001))
    {
        discard;
    }
    vec3 normal = normalize(v_Normal);
    if (!gl_FrontFacing)
    {
        normal = -normal;
    }
    vec3 sun = normalize(vec3(-0.4, 0.85, 0.55));
    vec3 worldNormal = normalize(vec3(u_Orientation.x * normal.x + u_Orientation.z * normal.z,
                                      normal.y,
                                      u_Orientation.y * normal.x + u_Orientation.w * normal.z));
    float diffuse = max(dot(worldNormal, sun), 0.0);
    vec3 lighting = vec3(0.27, 0.30, 0.35) + vec3(0.75, 0.68, 0.56) * diffuse;
    vec3 delta = u_LampPosition.xyz - v_Position;
    float distanceSquared = dot(delta, delta);
    float rangeFade = u_LampPosition.w > 0.0
                          ? clamp(1.0 - sqrt(distanceSquared) / u_LampPosition.w, 0.0, 1.0)
                          : 1.0;
    // Photometric-to-scene exposure calibration for the authored lantern.
    lighting += u_LampColor.rgb * u_LampColor.a * 0.002 * rangeFade
                * max(dot(normal, normalize(delta + vec3(0.00001))), 0.0)
                / (1.0 + distanceSquared * 4.0);
    float highlight = pow(max(dot(worldNormal, normalize(sun + vec3(1, 1, 1))), 0.0),
                          mix(64.0, 4.0, clamp(v_Surface.y, 0.0, 1.0)));
    vec3 base = v_Color.rgb * u_Tint.rgb;
    vec3 specular = mix(vec3(0.04), base, clamp(v_Surface.z, 0.0, 1.0)) * highlight;
    vec3 shaded = base * lighting + specular;
    vec3 color = mix(shaded, base, v_Emission.a) + v_Emission.rgb;
    o_Color = vec4(u_LinearScene ? color : linearToSrgb(color), v_Color.a * u_Tint.a);
}
