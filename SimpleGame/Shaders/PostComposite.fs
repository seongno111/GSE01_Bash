#version 330 core
in vec2 v_UV;
out vec4 FragColor;
uniform sampler2D u_Source;
uniform sampler2D u_Bloom;
uniform sampler2D u_BlurredScene;
uniform float u_Exposure;
uniform float u_BloomStrength;
uniform float u_VignetteStrength;
uniform float u_VignetteStart;
uniform float u_BlurStrength;
uniform float u_BlurStart;

vec3 toneMap(vec3 x)
{
    // Filmic approximation of the ACES response, not the full ACES pipeline.
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

vec3 linearToSRGB(vec3 x)
{
    vec3 low = x * 12.92;
    vec3 high = 1.055 * pow(max(x, vec3(0)), vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), x));
}

void main()
{
    // Screen-relative ellipse: center 0, corners 1. No depth-of-field claim.
    float radius = length((v_UV - 0.5) * 2.0) / sqrt(2.0);
    float blurMask = smoothstep(u_BlurStart, 1.0, radius);
    vec3 hdr = mix(texture(u_Source, v_UV).rgb,
                   texture(u_BlurredScene, v_UV).rgb,
                   blurMask * u_BlurStrength);
    hdr += texture(u_Bloom, v_UV).rgb * u_BloomStrength;
    float vignette = 1.0 - u_VignetteStrength * smoothstep(u_VignetteStart, 1.0, radius);
    hdr *= max(vignette, 0.0);
    vec3 color = linearToSRGB(toneMap(max(hdr * u_Exposure, vec3(0))));
    // Subtle one-LSB dither reduces banding in the smooth dark falloff.
    float noise = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
    FragColor = vec4(clamp(color + (noise - 0.5) / 255.0, 0.0, 1.0), 1.0);
}
