#include "stdafx.h"
#include "FireVfx.h"
#include "Prototype.h"
#include "Renderer.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float Pi = 3.14159265f;
    constexpr float Radians = Pi / 180;

    WorldPoint Forward(WorldPoint origin, double yaw, float distance)
    {
        return {origin.x + std::sin(yaw) * distance, origin.y + std::cos(yaw) * distance};
    }

    std::array<float, 4> FireColor(float alpha)
    {
        return {4.5f, .85f, .065f, alpha};
    }
} // namespace

FireVfx::FireVfx()
{
    m_Instances.reserve(InstanceCapacity);
}

float FireVfx::Random()
{
    // Visual RNG has no relationship to combat, map generation, or NPC decisions.
    m_Random ^= m_Random << 13;
    m_Random ^= m_Random >> 17;
    m_Random ^= m_Random << 5;
    return float(m_Random & 0xffffff) / 16777216.0f;
}

void FireVfx::Update(float dt)
{
    if (!std::isfinite(dt) || dt <= 0)
    {
        return;
    }
    for (auto& p : m_Particles)
    {
        if (p.life <= 0)
        {
            continue;
        }
        p.life -= dt;
        p.position.x += p.velocity.x * dt;
        p.position.y += p.velocity.y * dt;
        p.height += p.lift * dt;
        p.lift -= (p.shape == 1 ? 3.0f : -.3f) * dt;
        if (p.height < 0)
        {
            p.life = 0;
        }
    }
    for (auto& e : m_Effects)
    {
        e.life = (std::max)(0.0f, e.life - dt);
    }
}

FireVfx::Particle* FireVfx::NewParticle()
{
    size_t searched = 0;
    while (m_Particles[m_ParticleCursor].life > 0 && searched < ParticleCapacity)
    {
        m_ParticleCursor = (m_ParticleCursor + 1) % ParticleCapacity;
        ++searched;
    }
    if (searched == ParticleCapacity)
    {
        ++m_Dropped;
        return nullptr;
    }
    auto& particle = m_Particles[m_ParticleCursor];
    m_ParticleCursor = (m_ParticleCursor + 1) % ParticleCapacity;
    particle = {};
    return &particle;
}

void FireVfx::Burst(WorldPoint position, float height, int count, float speed)
{
    for (int i = 0; i < count; ++i)
    {
        auto* particle = NewParticle();
        if (!particle)
        {
            return;
        }
        auto& p = *particle;
        const float angle = Random() * 2 * Pi;
        const float velocity = (.25f + Random() * .75f) * speed;
        p.position = position;
        p.position.x += std::sin(angle) * .04;
        p.position.y += std::cos(angle) * .04;
        p.velocity = {std::sin(angle) * velocity, std::cos(angle) * velocity};
        p.height = height;
        p.lift = .3f + Random() * speed;
        p.duration = .25f + Random() * .5f;
        p.life = p.duration;
        p.size = .045f + Random() * .10f;
        p.seed = Random() * 100;
        p.shape = i % 4 == 0 ? 0 : 1;
        if (i % 7 == 0 && count >= 10)
        {
            p.shape = 6;
            p.duration = p.life = .8f;
            p.size = .22f;
            p.lift *= .3f;
        }
    }
}

void FireVfx::SlashFlames(WorldPoint origin, double yaw, float radius, float halfAngle, bool skill)
{
    const int count = skill ? 32 : 18;
    for (int i = 0; i < count; ++i)
    {
        auto* particle = NewParticle();
        if (!particle)
        {
            return;
        }
        auto& p = *particle;
        const double angle = yaw + (Random() * 2 - 1) * halfAngle;
        p.position = Forward(origin, angle, radius * (.65f + .25f * Random()));
        const float tangent = (skill ? 1.8f : 1.1f) * (.5f + Random());
        // Carry the fire along the blade sweep and let the ends peel outward.
        p.velocity = {std::cos(angle) * tangent + std::sin(angle) * .35,
                      -std::sin(angle) * tangent + std::cos(angle) * .35};
        p.height = .65f + Random() * .35f;
        p.lift = .4f + Random() * .9f;
        p.duration = .3f + Random() * .45f;
        p.life = p.duration;
        p.size = .09f + Random() * .13f;
        p.seed = Random() * 100;
        p.shape = i % 3 == 0 ? 1 : 0;
    }
}

void FireVfx::AddEffect(WorldPoint position,
                        double yaw,
                        float radius,
                        float halfAngle,
                        float height,
                        float duration,
                        int shape)
{
    size_t searched = 0;
    while (m_Effects[m_EffectCursor].life > 0 && searched < EffectCapacity)
    {
        m_EffectCursor = (m_EffectCursor + 1) % EffectCapacity;
        ++searched;
    }
    if (searched == EffectCapacity)
    {
        ++m_Dropped;
        return;
    }
    auto& e = m_Effects[m_EffectCursor];
    m_EffectCursor = (m_EffectCursor + 1) % EffectCapacity;
    e = {position, yaw, height, radius, halfAngle, duration, duration, shape};
}

void FireVfx::Hit(WorldPoint position, float height, float strength)
{
    Burst(position, height, int(9 * strength), 1.3f * strength);
    AddEffect(position, 0, .3f * strength, Pi, height, .22f, 2);
}

void FireVfx::Trail(WorldPoint position, double dx, double dy, bool arrow)
{
    Burst(position, .98f, 2, .18f);
    AddEffect(position, std::atan2(dx, dy), arrow ? .25f : .38f, Pi, .98f, .12f, 7);
}

void FireVfx::WeaponContact(HeroSkillKind kind,
                            WorldPoint origin,
                            double yaw,
                            float range,
                            float width,
                            bool skill,
                            int contact)
{
    switch (kind)
    {
    case HeroSkillKind::AshField:
        if (skill)
        {
            AddEffect(origin, yaw, width, Pi, .02f, .55f, 4);
            AddEffect(origin, yaw, width, Pi, .04f, .45f, 2);
            for (int i = 0; i < 10; ++i)
            {
                const auto point = Forward(origin, Random() * 2 * Pi, Random() * width);
                Burst(point, .05f, 3, .45f);
            }
        }
        else
        {
            Hit(Forward(origin, yaw, .55f), 1.1f, .7f);
        }
        break;
    case HeroSkillKind::ArrowFan:
        Burst(Forward(origin, yaw, .6f), 1.0f, skill ? 15 : 6, .65f);
        AddEffect(origin, yaw, .7f, Pi, 1.0f, .18f, 7);
        break;
    case HeroSkillKind::GuardBreak:
        AddEffect(origin, yaw, range, (skill ? width : 35) * Radians, .65f, .3f, skill ? 2 : 3);
        if (!skill)
        {
            SlashFlames(origin, yaw, range, 35 * Radians, false);
        }
        Burst(Forward(origin, yaw, 1.2f), .8f, skill ? 22 : 10, 1.6f);
        break;
    case HeroSkillKind::Execution:
        AddEffect(origin, yaw, range, (skill ? width : 65) * Radians, .85f, .35f, 3);
        SlashFlames(origin, yaw, range, (skill ? width : 65) * Radians, skill);
        break;
    case HeroSkillKind::GroundSlam:
    {
        const auto impact = Forward(origin, yaw, 1.0f);
        const float radius = range * .72f;
        AddEffect(impact, yaw, radius, Pi, .015f, skill ? 1.1f : .7f, 5);
        Burst(impact, .05f, skill ? 38 : 16, 2.0f);
        const int plumes = skill ? 7 : 3;
        for (int i = 0; i < plumes; ++i)
        {
            Burst(Forward(impact, Random() * 2 * Pi, (.15f + Random() * .6f) * radius),
                  .04f,
                  3,
                  .7f);
        }
    }
    break;
    case HeroSkillKind::LongThrust:
        AddEffect(origin, yaw, range, Pi, .9f, .25f, 7);
        Burst(Forward(origin, yaw, range * .7f), .9f, skill ? 16 : 6, .7f);
        break;
    case HeroSkillKind::Flurry:
        AddEffect(origin, yaw + (contact % 2 ? -.045 : .045), range, Pi, .9f, .16f, 8);
        Burst(Forward(origin, yaw, range * .85f), .9f, skill ? 8 : 5, .45f);
        break;
    case HeroSkillKind::Whirlwind:
        AddEffect(origin, yaw + contact * Pi, range, (skill ? 180 : 75) * Radians, .8f, .4f, 3);
        SlashFlames(origin, yaw + contact * Pi, range, (skill ? 180 : 75) * Radians, skill);
        break;
    }
}

void FireVfx::AddInstance(const FireVfxInstance& instance, const Renderer& renderer)
{
    const float extentX = std::abs(instance.xAxis[0]) + std::abs(instance.yAxis[0]);
    const float extentY = std::abs(instance.xAxis[1]) + std::abs(instance.yAxis[1]);
    if (instance.center[0] + extentX < 0 || instance.center[1] + extentY < 0
        || instance.center[0] - extentX > renderer.Width()
        || instance.center[1] - extentY > renderer.Height())
    {
        return;
    }
    if (m_Instances.size() < InstanceCapacity)
    {
        m_Instances.push_back(instance);
    }
    else
    {
        ++m_Dropped;
    }
}

void FireVfx::Draw(const Prototype& game, Renderer& renderer)
{
    m_Instances.clear();
    const float zoom = game.Zoom();
    const auto project = [&](WorldPoint point, float height)
    {
        auto p = game.Project(point.x, point.y);
        p.y -= height * 40 * zoom;
        return std::array<float, 2>{p.x, p.y};
    };
    const auto ground = [&](WorldPoint origin,
                            double yaw,
                            float radius,
                            float height,
                            int shape,
                            float phase,
                            float halfAngle,
                            float opacity)
    {
        FireVfxInstance instance;
        instance.center = project(origin, height);
        const float sideX = float(std::cos(yaw));
        const float sideY = -float(std::sin(yaw));
        const float forwardX = float(std::sin(yaw));
        const float forwardY = float(std::cos(yaw));
        instance.xAxis = {(sideX - sideY) * 40 * radius * zoom,
                          (sideX + sideY) * 20 * radius * zoom};
        instance.yAxis = {(forwardX - forwardY) * 40 * radius * zoom,
                          (forwardX + forwardY) * 20 * radius * zoom};
        instance.color = FireColor(opacity);
        instance.shape = {float(shape), phase, halfAngle, 3};
        AddInstance(instance, renderer);
    };

    // A small charcoal haze goes behind the emissive shapes.
    const auto particles = [&](bool smoke)
    {
        for (const auto& p : m_Particles)
        {
            if (p.life <= 0 || (p.shape == 6) != smoke)
            {
                continue;
            }
            const float phase = 1 - p.life / p.duration;
            const float fade = (1 - phase) * (1 - phase);
            const float size = p.size * 40 * zoom * (smoke ? 1 + phase * 2 : 1);
            FireVfxInstance instance;
            instance.center = project(p.position, p.height);
            instance.xAxis = {size * (p.shape == 1 ? .18f : 1), 0};
            instance.yAxis = {0, size * (p.shape == 1 ? 2.3f : 1.7f)};
            if (p.shape == 1)
            {
                const float dx = float(p.velocity.x - p.velocity.y) * 40;
                const float dy = float(p.velocity.x + p.velocity.y) * 20 - p.lift * 40;
                const float length = (std::max)(.001f, std::hypot(dx, dy));
                instance.xAxis = {-dy / length * size * .18f, dx / length * size * .18f};
                instance.yAxis = {dx / length * size * 2.3f, dy / length * size * 2.3f};
            }
            instance.color = smoke ? std::array<float, 4>{.025f, .015f, .012f, fade * .30f}
                                   : FireColor(fade * .8f);
            instance.shape = {float(p.shape), phase, Pi, p.seed};
            AddInstance(instance, renderer);
        }
    };
    particles(true);
    for (const auto& e : m_Effects)
    {
        if (e.life <= 0)
        {
            continue;
        }
        const float phase = 1 - e.life / e.duration;
        const float radius = e.shape == 4 || e.shape == 5 || e.shape == 7 || e.shape == 8
                                 ? e.radius
                                 : e.radius * (.45f + .55f * phase);
        if (e.shape == 7 || e.shape == 8)
        {
            // Project a narrow strip along the actual world-space thrust/flight direction.
            const auto a = project(e.position, e.height);
            const auto b = project(Forward(e.position, e.yaw, radius), e.height);
            const float dx = b[0] - a[0];
            const float dy = b[1] - a[1];
            const float length = (std::max)(.001f, std::hypot(dx, dy));
            FireVfxInstance instance;
            instance.center = {(a[0] + b[0]) * .5f, (a[1] + b[1]) * .5f};
            const float thickness = (e.shape == 8 ? 2.5f : 5.0f) * zoom;
            instance.xAxis = {-dy / length * thickness, dx / length * thickness};
            instance.yAxis = {dx * .5f, dy * .5f};
            instance.color = FireColor((1 - phase) * .9f);
            instance.shape = {e.shape == 8 ? 8.0f : 1.0f, phase, Pi, 0};
            AddInstance(instance, renderer);
        }
        else
        {
            ground(e.position,
                   e.yaw,
                   radius,
                   e.height,
                   e.shape,
                   phase,
                   e.halfAngle,
                   (1 - phase) * .8f);
        }
    }

    const auto& player = game.Player();
    const auto& skill = HeroWeapons[player.weapon].skill;
    const auto& cast = player.skillCast;
    if (cast.startedAt < 0 && player.attackReleaseAt >= 0)
    {
        const float elapsed = game.Time() - player.weaponAttackStart;
        const float duration = player.attackReleaseAt - player.weaponAttackStart;
        const float charge = std::clamp(elapsed / (std::max)(.001f, duration), 0.0f, 1.0f);
        const auto position = Forward(player.WorldPosition(), player.ModelYaw(), .45f);
        FireVfxInstance flame;
        flame.center = project(position, 1.05f);
        flame.xAxis = {(3 + charge * 3) * zoom, 0};
        flame.yAxis = {0, (6 + charge * 6) * zoom};
        flame.color = FireColor(.35f + .35f * charge);
        flame.shape = {0, charge, Pi, 2};
        AddInstance(flame, renderer);
    }
    if (cast.startedAt >= 0)
    {
        const float elapsed = game.Time() - cast.startedAt;
        const float charge =
            cast.contacts.empty() ? 0 : std::clamp(elapsed / cast.contacts.front(), 0.0f, 1.0f);
        if (cast.nextHit == 0)
        {
            const auto center =
                skill.kind == HeroSkillKind::AshField ? cast.center : player.WorldPosition();
            const float radius =
                skill.kind == HeroSkillKind::AshField ? skill.width : .5f + charge * .3f;
            ground(center, cast.yaw, radius, .02f, 4, charge, Pi, .2f + .3f * charge);
        }
    }
    particles(false);
    renderer.DrawFireVfx(m_Instances, m_Dropped);
}
