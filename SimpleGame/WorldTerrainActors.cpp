#include "stdafx.h"
#include "Prototype.h"

#include <cmath>
#include <limits>
#include <utility>

WorldChunkActor::WorldChunkActor(Prototype& game,
                                 Terrain::ChunkPlan plan,
                                 std::array<WorldTileData, 64> tiles)
    : GameActor(game, "Terrain/" + plan.ground.key, TickGroup::None, ActorLayer::Terrain),
      m_Plan(std::move(plan)),
      m_Tiles(std::move(tiles))
{
}

bool WorldChunkActor::InView() const
{
    const Point center = m_Game.Project(WorldX() + 3.5, WorldY() + 3.5);
    const float margin = 420 * m_Game.Zoom();
    return center.x >= -margin && center.x <= m_Game.m_Width + margin && center.y >= -margin
           && center.y <= m_Game.m_Height + margin;
}

void WorldChunkActor::Draw(Renderer& renderer)
{
    ModelGroundMask mask;
    mask.hidden = {std::uint32_t(m_Plan.hiddenTiles), std::uint32_t(m_Plan.hiddenTiles >> 32)};
    mask.offset = {float(m_Plan.offset.x), float(m_Plan.offset.y)};
    if (!renderer.DrawModel(m_Plan.ground.model,
                            nullptr,
                            m_Game.Project(WorldX() + m_Plan.offset.x, WorldY() + m_Plan.offset.y),
                            m_Game.Zoom(),
                            m_Plan.yaw,
                            false,
                            Color(),
                            &mask))
    {
        for (size_t i = 0; i < m_Tiles.size(); ++i)
        {
            if (!(m_Plan.hiddenTiles & (std::uint64_t(1) << i)))
            {
                const auto& tile = m_Tiles[i];
                m_Game.DrawTile(renderer, tile, m_Game.Project(double(tile.x), double(tile.y)));
            }
        }
    }
    if (m_Game.GridEnabled())
    {
        const Color color(.5f, .78f, .7f, .6f);
        for (int side = 0; side < 4; ++side)
        {
            const WorldPoint corners[] = {{-.5, -.5}, {7.5, -.5}, {7.5, 7.5}, {-.5, 7.5}};
            const auto a = corners[side];
            const auto b = corners[(side + 1) % 4];
            renderer.Line(m_Game.Project(WorldX() + a.x, WorldY() + a.y),
                          m_Game.Project(WorldX() + b.x, WorldY() + b.y),
                          1,
                          color);
        }
    }
}

TerrainPropActor::TerrainPropActor(Prototype& game, Terrain::Prop prop)
    : GameActor(game, "TerrainProp/" + prop.key, TickGroup::None, ActorLayer::World),
      m_Prop(std::move(prop))
{
    SetLocalPosition(m_Prop.position.x, m_Prop.position.y);
}

double TerrainPropActor::ColliderRadius() const
{
    return m_Prop.radius;
}

void TerrainPropActor::Draw(Renderer& renderer)
{
    auto feet = m_Game.Project(WorldX(), WorldY());
    feet.y -= m_Prop.elevation * 40 * m_Game.Zoom();
    if (m_Prop.canopy)
    {
        const auto player = m_Game.Project(m_Game.Player().WorldX(), m_Game.Player().WorldY());
        const float zoom = m_Game.Zoom();
        if (std::abs(player.x - feet.x) < 145 * zoom && player.y > feet.y - 240 * zoom
            && player.y < feet.y + 30 * zoom)
        {
            return;
        }
    }
    if (!renderer.DrawModel(m_Prop.model, nullptr, feet, m_Game.Zoom(), m_Prop.yaw, false))
    {
        renderer.Ellipse(feet, 18 * m_Game.Zoom(), 10 * m_Game.Zoom(), Color(.25f, .28f, .25f));
    }
}
