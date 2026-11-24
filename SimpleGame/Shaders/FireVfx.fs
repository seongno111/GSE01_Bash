#version 330 core
in vec2 v_Local;
flat in vec4 v_Color;
flat in vec4 v_Shape;
uniform bool u_LinearScene;
out vec4 color;

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}
float noise(vec2 p)
{
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1,0)), f.x),
               mix(hash(i + vec2(0,1)), hash(i + vec2(1,1)), f.x), f.y);
}

float segmentDistance(vec2 p, vec2 a, vec2 b)
{
    vec2 edge = b - a;
    float t = clamp(dot(p - a, edge) / max(dot(edge, edge), .00001), 0.0, 1.0);
    return length(p - a - edge * t);
}

float fissureDistance(vec2 p)
{
    float distance = 1.0;
    for (int i = 0; i < 6; ++i)
    {
        float seed = float(i) + v_Shape.w;
        float angle = float(i) * 1.0472 + (hash(vec2(seed, 2)) - .5) * .55;
        vec2 direction = vec2(sin(angle), cos(angle));
        vec2 side = vec2(direction.y, -direction.x);
        vec2 joint = direction * (.32 + hash(vec2(seed, 3)) * .16)
                     + side * (hash(vec2(seed, 5)) - .5) * .22;
        vec2 tip = direction * (.68 + hash(vec2(seed, 7)) * .24)
                   + side * (hash(vec2(seed, 11)) - .5) * .23;
        vec2 spur = joint + direction * .24 + side * (.17 + hash(vec2(seed, 13)) * .16);
        distance = min(distance, segmentDistance(p, vec2(0), joint));
        distance = min(distance, segmentDistance(p, joint, tip));
        distance = min(distance, segmentDistance(p, joint, spur));
    }
    return distance;
}

void main()
{
    vec2 p = v_Local;
    float phase = v_Shape.y;
    float radius = length(p);
    float angle = atan(p.x, p.y);
    float mask = 0;
    float core = 0;
    float turbulence = noise(p * 8.0 + vec2(v_Shape.w, -phase * 8.0));
    int shape = int(v_Shape.x + 0.5);
    if (shape == 0)
    {
        // A pinched flame with upward-moving turbulence, soft edges and a hot heart.
        p.x += sin(p.y * 5.0 + phase * 9.0 + v_Shape.w) * .14 * (1.0 - p.y);
        float body = length(vec2(p.x * (1.15 - .35 * p.y), p.y));
        mask = (1.0 - smoothstep(.2, 1.0, body + turbulence * .22));
        core = pow(max(0.0, 1.0 - body * 1.8), 2.0);
    }
    else if (shape == 1)
    {
        mask = (1.0 - smoothstep(.05, 1.0, abs(p.x))) *
               (1.0 - smoothstep(.3, 1.0, abs(p.y)));
        core = .65;
    }
    else if (shape == 6)
    {
        mask = (1.0 - smoothstep(.2, 1.0, radius)) * (.4 + turbulence * .6);
    }
    else if (shape == 8)
    {
        // A narrow thrust narrows to a hot point at the far end of the strip.
        float forward = (p.y + 1.0) * .5;
        float width = mix(.85, .06, forward);
        mask = (1.0 - smoothstep(width * .25, width, abs(p.x)))
               * smoothstep(0.0, .12, forward) * (1.0 - smoothstep(.92, 1.0, forward));
        core = .85;
    }
    else
    {
        float sector = 1.0 - smoothstep(v_Shape.z - .08, v_Shape.z, abs(angle));
        if (v_Shape.z > 3.13) sector = 1.0;
        float ring = 1.0 - smoothstep(.02, .10, abs(radius - .82 + .025 * turbulence));
        if (shape == 2)
            mask = ring * sector;
        else if (shape == 3)
        {
            float sweep = clamp((angle / max(.1, v_Shape.z) + 1.0) * .5, 0.0, 1.0);
            float flameEdge = .76 + .13 * turbulence + .035 * sin(angle * 15.0 - phase * 18.0);
            float flame = (1.0 - smoothstep(.025, .14, abs(radius - flameEdge)))
                          * (.35 + .65 * turbulence);
            mask = max(ring * .7, flame) * sector * (.25 + .75 * sweep);
        }
        else if (shape == 4)
        {
            float inner = 1.0 - smoothstep(.012, .035, abs(radius - .53));
            float spokes = pow(max(0.0, cos(angle * 8.0)), 28.0) *
                           smoothstep(.54, .62, radius) * (1.0 - smoothstep(.75, .84, radius));
            mask = max(ring, max(inner, spokes));
        }
        else if (shape == 5)
        {
            // Stable jagged branches open outward from the hammer's contact point.
            float crack = fissureDistance(p);
            float reveal = 1.0 - smoothstep(clamp(phase * 4.5, 0.0, 1.0),
                                          clamp(phase * 4.5, 0.0, 1.0) + .06, radius);
            mask = (1.0 - smoothstep(.012, .04, crack)) * reveal;
            core = (1.0 - smoothstep(.003, .013, crack)) * reveal;
        }
        if (shape != 5) core = mask * .7;
    }
    float alpha = clamp(mask * v_Color.a, 0.0, 1.0);
    if (alpha < .002) discard;
    vec3 rgb = mix(v_Color.rgb, vec3(7.0, 3.5, .7), core * .7);
    if (shape == 5) rgb = mix(vec3(.025, .012, .008), rgb, core);
    if (!u_LinearScene) rgb = vec3(1.0) - exp(-rgb);
    // Premultiplied alpha preserves warm HDR edges without dark fringes.
    color = vec4(rgb * alpha, alpha);
}
