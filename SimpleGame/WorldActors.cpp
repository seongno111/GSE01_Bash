#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"

#include <algorithm>
#include <cmath>

using namespace GameRules;

GameActor::GameActor(Prototype& game, const std::string& name, TickGroup tick, ActorLayer layer)
    : Actor(name, tick, layer),
      m_Game(game)
{
}

UnitSpritePose GameActor::SpritePose() const
{
    UnitSpritePose pose;
    const float now = m_Game.Time();
    pose.locomotionWeight = m_LocomotionWeight;
    pose.moving = m_LocomotionWeight > 0.001f;
    pose.walkCycle = m_WalkCycle;
    pose.facingLeft = m_FacingLeft;
    const float elapsed = now - m_AttackAnimationStart;
    if (m_AttackAnimationStart >= 0 && elapsed >= 0 && elapsed < m_AttackAnimationDuration)
    {
        pose.attackProgress = std::clamp(elapsed / m_AttackAnimationDuration, 0.0f, 1.0f);
        pose.facingLeft = m_AttackFacingLeft;
    }
    return pose;
}

void GameActor::RecordMovement(double dx, double dy)
{
    const double distance = std::hypot(dx, dy);
    if (distance <= 0.00001)
    {
        return;
    }
    const float now = m_Game.Time();
    // Accumulate collision-resolved travel. Visual time advances separately every
    // frame, so 20 Hz AI does not make limbs jump from one pose to the next.
    m_TravelDistance += float(distance);
    m_LastMovementTime = now;
    const double screenX = dx - dy;
    if (std::abs(screenX) > distance * 0.15)
    {
        m_FacingLeft = screenX < 0;
    }
}

void GameActor::UpdateVisual(float dt)
{
    if (!std::isfinite(dt) || dt <= 0)
    {
        return;
    }
    m_TravelWindow += dt;
    if (m_TravelWindow >= 0.10f)
    {
        m_TargetSpeed = m_TravelDistance / m_TravelWindow;
        m_TravelDistance = 0;
        m_TravelWindow = 0;
    }

    // Bridge one skipped path-node tick; settle smoothly when actually stopped.
    const bool active = m_LastMovementTime >= 0 && m_Game.Time() - m_LastMovementTime <= 0.12f;
    const float targetSpeed = active ? m_TargetSpeed : 0;
    m_VisualSpeed += (targetSpeed - m_VisualSpeed) * (1 - std::exp(-12 * dt));
    const float targetWeight = active ? 1.0f : 0.0f;
    m_LocomotionWeight += (targetWeight - m_LocomotionWeight) * (1 - std::exp(-14 * dt));
    if (!active && m_LocomotionWeight < 0.001f)
    {
        m_LocomotionWeight = 0;
        m_VisualSpeed = 0;
    }

    // One full cycle contains two alternating steps. Normal walking is about
    // 1 Hz; sprinting is capped at 1.8 Hz instead of the former nearly 3 Hz.
    constexpr float StrideLength = 3.6f;
    const float cyclesPerSecond = std::clamp(m_VisualSpeed / StrideLength, 0.0f, 1.8f);
    m_WalkCycle = std::fmod(m_WalkCycle + cyclesPerSecond * dt, 1.0f);
}

void GameActor::BeginAttackAnimation(double directionX, double directionY, float duration)
{
    const double screenX = directionX - directionY;
    if (std::abs(screenX) > 0.00001)
    {
        m_FacingLeft = screenX < 0;
    }
    m_AttackFacingLeft = m_FacingLeft;
    m_AttackAnimationStart = m_Game.Time();
    m_AttackAnimationDuration = (std::max)(0.01f, duration);
}

void GameActor::ResetAnimation()
{
    m_WalkCycle = 0;
    m_TravelWindow = 0;
    m_TravelDistance = 0;
    m_TargetSpeed = 0;
    m_VisualSpeed = 0;
    m_LocomotionWeight = 0;
    m_LastMovementTime = -1;
    m_AttackAnimationStart = -1;
    m_AttackAnimationDuration = 0;
    m_FacingLeft = false;
    m_AttackFacingLeft = false;
}

bool GameActor::InView() const
{
    const Point p = m_Game.Project(WorldX(), WorldY());
    const float margin = 250 * m_Game.m_Zoom;
    return p.x >= -margin && p.x <= m_Game.m_Width + margin && p.y >= -margin
           && p.y <= m_Game.m_Height + 400 * m_Game.m_Zoom;
}

TerrainActor::TerrainActor(Prototype& game, WorldTileData tile)
    : GameActor(game, "Terrain", TickGroup::None, ActorLayer::Ground),
      m_Tile(tile)
{
}

void TerrainActor::Draw(Renderer& r)
{
    m_Game.DrawTile(r, m_Tile, m_Game.Project(WorldX(), WorldY()));
}

PropActor::PropActor(Prototype& game, int kind, unsigned seed)
    : GameActor(game, "Prop", TickGroup::None, ActorLayer::World),
      m_Kind(kind),
      m_Seed(seed)
{
}

void PropActor::Draw(Renderer& r)
{
    m_Game.DrawProp(r, m_Game.Project(WorldX(), WorldY()), m_Kind, m_Seed);
}

double PropActor::ColliderRadius() const
{
    if (m_Kind == 7 || m_Kind == 8)
    {
        return 1.15;
    }
    if (m_Kind == 9)
    {
        return 0.6;
    }
    if (m_Kind == 5)
    {
        return 2.15;
    }
    if (m_Kind == 6)
    {
        return 0.55;
    }
    return m_Kind == 2 ? 0.62 : 0.36;
}

AtmosphereActor::AtmosphereActor(Prototype& game, bool ash, int index)
    : GameActor(game, ash ? "Ash" : "Fog", TickGroup::None, ActorLayer::Atmosphere),
      m_Ash(ash),
      m_Index(index)
{
}

void AtmosphereActor::Draw(Renderer& r)
{
    const int i = m_Index;
    const WorldPoint offset = WorldPosition();
    if (m_Ash)
    {
        const float x =
            std::fmod(float(i * 137) + m_Game.m_Time * (8 + i % 5), float(m_Game.m_Width));
        const float y =
            std::fmod(float(i * 83) + m_Game.m_Time * (3 + i % 3), float(m_Game.m_Height));
        r.Rect(x + float(offset.x),
               y + float(offset.y),
               1.5f,
               1.5f,
               Color(0.68f, 0.72f, 0.66f, 0.20f));
    }
    else
    {
        const float x = std::fmod(i * 311.0f + m_Game.m_Time * 9, m_Game.m_Width + 600.0f) - 300;
        const float y = float(i % 3) * m_Game.m_Height / 3.0f + 80;
        r.Glow({x + float(offset.x), y + float(offset.y)},
               360,
               90,
               Color(0.39f, 0.49f, 0.49f, 0.028f));
    }
}

RangeIndicatorActor::RangeIndicatorActor(Prototype& game)
    : GameActor(game, "Range", TickGroup::None, ActorLayer::Effects)
{
}

void RangeIndicatorActor::Draw(Renderer& r)
{
    if (m_Game.m_Player.autoFire || m_Game.m_Keys[' '])
    {
        for (int i = 0; i < 64; ++i)
        {
            const double angle = i * 6.283185307 / 64;
            const Point p = m_Game.Project(WorldX() + std::cos(angle) * m_Game.m_Player.range,
                                           WorldY() + std::sin(angle) * m_Game.m_Player.range);
            r.Rect(p.x, p.y, 2, 2, Color(0.76f, 0.65f, 0.38f, 0.35f));
        }
    }
}

HUDActor::HUDActor(Prototype& game)
    : GameActor(game, "HUD", TickGroup::None, ActorLayer::HUD)
{
}

void HUDActor::Draw(Renderer& r)
{
    const WorldPoint offset = WorldPosition();
    const Renderer::ScopedTranslation translation(r, {float(offset.x), float(offset.y)});
    m_Game.Interface(r);
}
