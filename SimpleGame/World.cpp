#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"
#include "Villages.h"
#include "Outposts.h"

#include <cmath>

using namespace GameRules;

WorldTileData Prototype::MakeTile(std::int64_t x, std::int64_t y) const
{
    const auto seed = Hash(x, y, m_WorldSeed);
    const double lx = double(x) - NearestShrine(double(x));
    const double ly = double(y) - NearestShrine(double(y));
    const bool plaza = std::abs(lx) <= 3 && std::abs(ly) <= 3;
    const bool road = std::abs(lx) <= 1 || std::abs(ly) <= 1 || plaza;
    float taint = SmoothNoise(double(x) / 7.0 + 17, double(y) / 7.0 - 29, m_WorldSeed);
    if (plaza)
    {
        taint *= 0.25f;
    }
    int prop = 0;
    // Expanded settlements use four-unit blocks and open cardinal gates.
    // Clear the entire village footprint; buildings leave connected streets.
    const int village = VillageAt({double(x), double(y)});
    if (village >= 0)
    {
        const auto vx = x - static_cast<std::int64_t>(Villages[village].center.x);
        const auto vy = y - static_cast<std::int64_t>(Villages[village].center.y);
        const bool houseX = std::abs(vx) == 6 || std::abs(vx) == 10 || std::abs(vx) == 14;
        const bool houseY = std::abs(vy) == 6 || std::abs(vy) == 10 || std::abs(vy) == 14;
        if (houseX && houseY)
        {
            prop = 7;
        }
        if ((std::abs(vx) == 6 || std::abs(vx) == 10) && vy == 2)
        {
            prop = 8;
        }
        if (std::abs(vx) == 2 && std::abs(vy) == 10)
        {
            prop = 9;
        }
        if ((std::abs(vx) == VillageHalfExtent - 1 && std::abs(vy) > 4 && vy % 3 == 0)
            || (std::abs(vy) == VillageHalfExtent - 1 && std::abs(vx) > 4 && vx % 3 == 0))
        {
            prop = 2;
        }
        return {x, y, taint * 0.15f, static_cast<unsigned>(seed >> 32), prop, true};
    }
    for (const auto& camp : Outposts)
    {
        const auto ox = x - std::int64_t(camp.center.x);
        const auto oy = y - std::int64_t(camp.center.y);
        if (std::abs(ox) <= OutpostHalfExtent && std::abs(oy) <= OutpostHalfExtent)
        {
            // Open entrances on every side; the spawn ring remains unobstructed.
            prop = ox == 0 && oy == 0 ? 10 : 0;
            if (((std::abs(ox) == OutpostHalfExtent - 1 && oy % 2 == 0)
                 || (std::abs(oy) == OutpostHalfExtent - 1 && ox % 2 == 0))
                && std::abs(ox) > 3 && std::abs(oy) > 3)
            {
                prop = 2;
            }
            if (std::abs(ox) == 9 && std::abs(oy) == 9)
            {
                prop = 7;
            }
            return {x, y, .75f, static_cast<unsigned>(seed >> 32), prop, false};
        }
    }
    // Keep the landmark and its approach clear, including the respawn point.
    if (!road && x % 4 != 0 && y % 4 != 0 && !(std::abs(lx) < 6 && std::abs(ly) < 6))
    {
        if (seed % 19 == 0)
        {
            prop = 1; // dead tree
        }
        else if (seed % 29 == 0)
        {
            prop = 2; // broken wall
        }
        else if (seed % 13 == 0)
        {
            prop = 3; // rock
        }
        else if (seed % 37 == 0)
        {
            prop = 4; // grave marker
        }
    }
    const int cell = LevelOneCell(x, y);
    if (cell >= 0 && !m_LevelWalkable.empty())
    {
        // Connected walkable cells are clear; unreachable pockets become solid terrain.
        const bool traversable = road || m_LevelWalkable[cell];
        prop = traversable ? 0 : 2;
        return {x,
                y,
                taint * 0.7f,
                static_cast<unsigned>(seed >> 32),
                prop,
                road || (traversable && (y == 3 || x % 3 == 0))};
    }

    return {x, y, taint, static_cast<unsigned>(seed >> 32), prop, road};
}

void Prototype::Stream()
{
    const auto cx = ChunkOf(m_Player.WorldX());
    const auto cy = ChunkOf(m_Player.WorldY());
    m_Radius = int(std::ceil((m_Width / 160.0 + m_Height / 80.0 + 8) / m_Zoom / ChunkSize)) + 1;
    if (m_StreamRadius == m_Radius && m_StreamCenter == ChunkKey{cx, cy})
    {
        return;
    }
    m_StreamCenter = {cx, cy};
    m_StreamRadius = m_Radius;
    for (auto it = m_Chunks.begin(); it != m_Chunks.end();)
    {
        if (std::abs(it->first.first - cx) > m_Radius || std::abs(it->first.second - cy) > m_Radius)
        {
            m_Scene.Destroy(it->second);
            it = m_Chunks.erase(it);
        }
        else
        {
            ++it;
        }
    }
    for (auto y = cy - m_Radius; y <= cy + m_Radius; ++y)
    {
        for (auto x = cx - m_Radius; x <= cx + m_Radius; ++x)
        {
            const ChunkKey key{x, y};
            if (m_Chunks.find(key) != m_Chunks.end())
            {
                continue;
            }
            ::Actor& chunk = m_Scene.Spawn<::Actor>(m_WorldRoot, "Chunk");
            chunk.SetLocalPosition(double(x * ChunkSize), double(y * ChunkSize));
            for (int ty = 0; ty < ChunkSize; ++ty)
            {
                for (int tx = 0; tx < ChunkSize; ++tx)
                {
                    const WorldTileData tile = MakeTile(x * ChunkSize + tx, y * ChunkSize + ty);
                    auto& terrain = m_Scene.Spawn<TerrainActor>(chunk.Id(), *this, tile);
                    terrain.SetLocalPosition(tx, ty);
                    if (tile.prop)
                    {
                        m_Scene.Spawn<PropActor>(terrain.Id(), *this, tile.prop, tile.variant);
                    }
                    if (tile.x % ShrineSpacing == 0 && tile.y % ShrineSpacing == 0)
                    {
                        m_Scene.Spawn<PropActor>(terrain.Id(), *this, 6);
                        auto& chapel = m_Scene.Spawn<PropActor>(terrain.Id(), *this, 5);
                        chapel.SetLocalPosition(-3, -3);
                    }
                }
            }
            m_Chunks.emplace(key, chunk.Id());
        }
    }
}

bool Prototype::Blocked(double x, double y) const
{
    if (m_Scene.Blocked(x, y))
    {
        return true;
    }
    // Loaded terrain uses actor colliders, including moved/disabled props.
    // Keep deterministic queries for distant, not-yet-streamed terrain.
    bool loaded = true;
    for (const double px : {x - 3, x + 3})
    {
        for (const double py : {y - 3, y + 3})
        {
            if (m_Chunks.find({ChunkOf(px), ChunkOf(py)}) == m_Chunks.end())
            {
                loaded = false;
            }
        }
    }
    if (loaded)
    {
        return false;
    }
    // A reproducible query independent of whether a chunk is currently loaded.
    const double sx = NearestShrine(x);
    const double sy = NearestShrine(y);
    if (Distance(x - (sx - 3), y - (sy - 3)) < 2.15)
    {
        return true;
    }
    if (Distance(x - sx, y - sy) < 0.55)
    {
        return true;
    }
    const auto ix = static_cast<std::int64_t>(std::round(x));
    const auto iy = static_cast<std::int64_t>(std::round(y));
    for (auto ty = iy - 1; ty <= iy + 1; ++ty)
    {
        for (auto tx = ix - 1; tx <= ix + 1; ++tx)
        {
            const WorldTileData t = MakeTile(tx, ty);
            const double radius = t.prop == 7 || t.prop == 8 ? 1.15
                                  : t.prop == 9              ? 0.6
                                  : t.prop == 2              ? 0.62
                                                             : 0.36;
            if (t.prop && Distance(x - double(tx), y - double(ty)) < radius)
            {
                return true;
            }
        }
    }
    return false;
}

void Prototype::Move(double& x, double& y, double dx, double dy, float radius)
{
    const auto free = [this, radius](double px, double py)
    {
        return !Blocked(px - radius, py) && !Blocked(px + radius, py) && !Blocked(px, py - radius)
               && !Blocked(px, py + radius);
    };
    if (free(x + dx, y))
    {
        x += dx;
    }
    if (free(x, y + dy))
    {
        y += dy;
    }
}

void Prototype::MoveActor(::Actor& actor, double dx, double dy, float radius)
{
    const WorldPoint previous = actor.WorldPosition();
    WorldPoint position = previous;
    Move(position.x, position.y, dx, dy, radius);
    actor.SetWorldPosition(position.x, position.y);
    if (auto* unit = dynamic_cast<GameActor*>(&actor))
    {
        unit->RecordMovement(position.x - previous.x, position.y - previous.y);
    }
}
