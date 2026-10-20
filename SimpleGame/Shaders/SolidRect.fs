#version 330 core
in vec4 v_Color;
in vec2 v_SpriteUV;
in vec2 v_LocalUV;
flat in int v_SpriteCode;
flat in float v_Radiance;
flat in vec4 v_EmissionBounds;
uniform sampler2D u_SpriteAtlas;
uniform bool u_LinearScene;
layout(location = 0) out vec4 FragColor;

vec3 sRGBToLinear(vec3 x)
{
    vec3 low = x / 12.92;
    vec3 high = pow(max((x + 0.055) / 1.055, vec3(0)), vec3(2.4));
    return mix(low, high, step(vec3(0.04045), x));
}

float emissionRegion(vec4 bounds)
{
    return all(greaterThanEqual(v_LocalUV, bounds.xy)) && all(lessThanEqual(v_LocalUV, bounds.zw))
               ? 1.0
               : 0.0;
}

float spriteEmission(vec3 color)
{
    // These small regions belong to realistic-atlas-v1's alpha-cropped artwork.
    // Warm color alone would incorrectly make leather, faces and gold emit light.
    float region = 0.0;
    if (v_SpriteCode == 1) // Player's lantern glass.
    {
        region = emissionRegion(vec4(0.86, 0.52, 0.98, 0.61));
    }
    else if (v_SpriteCode == 11) // Cathedral stained glass.
    {
        region = emissionRegion(vec4(0.52, 0.55, 0.67, 0.70))
                 + emissionRegion(vec4(0.25, 0.54, 0.31, 0.64))
                 + emissionRegion(vec4(0.27, 0.72, 0.32, 0.80))
                 + emissionRegion(vec4(0.77, 0.54, 0.82, 0.73));
    }
    else if (v_SpriteCode == 12) // Cottage windows.
    {
        region = emissionRegion(vec4(0.30, 0.44, 0.39, 0.56))
                 + emissionRegion(vec4(0.69, 0.55, 0.79, 0.68))
                 + emissionRegion(vec4(0.69, 0.70, 0.78, 0.80));
    }
    else if (v_EmissionBounds.z > v_EmissionBounds.x && v_EmissionBounds.w > v_EmissionBounds.y)
    {
        // Startup validation finds only bright lantern pixels in the player's
        // right-hand area. The bounds travel with each pose and mirror with UVs.
        region = emissionRegion(v_EmissionBounds);
    }
    else
    {
        return 0.0;
    }

    float warm =
        smoothstep(0.10, 0.22, color.r - color.g) * smoothstep(0.06, 0.18, color.g - color.b);
    float bright = smoothstep(0.50, 0.75, color.r);
    return min(region, 1.0) * warm * bright;
}

void main()
{
    FragColor = v_Color;
    if (v_SpriteCode != 0)
    {
        // WIC decodes straight-alpha sRGB pixels. Alpha remains linear and the
        // same SRC_ALPHA blending used by geometry preserves painter ordering.
        vec4 texel = texture(u_SpriteAtlas, v_SpriteUV);
        if (texel.a <= 8.0 / 255.0)
        {
            discard;
        }
        FragColor *= texel;
        if (u_LinearScene)
        {
            FragColor.rgb =
                sRGBToLinear(FragColor.rgb) * v_Radiance * (1.0 + 5.0 * spriteEmission(texel.rgb));
        }
    }
}
