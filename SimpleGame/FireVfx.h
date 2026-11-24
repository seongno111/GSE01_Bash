#pragma once
#include "Actor.h"
#include "HeroWeapons.h"
#include <array>
#include <cstdint>
#include <cstddef>
#include <vector>

class Prototype;
class Renderer;

// One quad per particle or analytic effect. Screen axes include camera zoom/projection.
struct FireVfxInstance
{
    std::array<float, 2> center{};
    std::array<float, 2> xAxis{};
    std::array<float, 2> yAxis{};
    std::array<float, 4> color{};
    // Shape, animation phase, sector half-angle, fixed visual seed.
    std::array<float, 4> shape{};
};

class FireVfx
{
public:

    static constexpr size_t ParticleCapacity = 768;
    static constexpr size_t EffectCapacity = 64;
    static constexpr size_t InstanceCapacity = 1024;

    FireVfx();
    void Update(float dt);
    void WeaponContact(HeroSkillKind kind,
                       WorldPoint origin,
                       double yaw,
                       float range,
                       float width,
                       bool skill,
                       int contact = 0);
    void Hit(WorldPoint position, float height = .85f, float strength = 1);
    void Trail(WorldPoint position, double dx, double dy, bool arrow);
    void Draw(const Prototype& game, Renderer& renderer);

private:

    struct Particle
    {
        WorldPoint position;
        WorldPoint velocity;
        float height = 0;
        float lift = 0;
        float life = 0;
        float duration = 0;
        float size = 0;
        float seed = 0;
        int shape = 0;
    };

    struct Effect
    {
        WorldPoint position;
        double yaw = 0;
        float height = 0;
        float radius = 0;
        float halfAngle = 3.14159265f;
        float life = 0;
        float duration = 0;
        int shape = 2;
    };

    float Random();
    Particle* NewParticle();
    void Burst(WorldPoint position, float height, int count, float speed);
    void SlashFlames(WorldPoint origin, double yaw, float radius, float halfAngle, bool skill);
    void AddEffect(WorldPoint position,
                   double yaw,
                   float radius,
                   float halfAngle,
                   float height,
                   float duration,
                   int shape);
    void AddInstance(const FireVfxInstance& instance, const Renderer& renderer);

    std::array<Particle, ParticleCapacity> m_Particles{};
    std::array<Effect, EffectCapacity> m_Effects{};
    std::vector<FireVfxInstance> m_Instances;
    size_t m_ParticleCursor = 0;
    size_t m_EffectCursor = 0;
    std::uint32_t m_Random = 0x7f4a7c15;
    std::uint64_t m_Dropped = 0;
};
