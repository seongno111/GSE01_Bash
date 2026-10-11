#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"

#include <cmath>

using namespace GameRules;

GameActor::GameActor(Prototype& game, const std::string& name, TickGroup tick, ActorLayer layer)
    : Actor(name, tick, layer),
      m_Game(game)
{
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
