#include "stdafx.h"
#include "WorldTerrain.h"
#include "GameRules.h"
#include "Villages.h"
#include "Outposts.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace Terrain
{
    static_assert(GameRules::ChunkSize == 8, "Authored terrain requires eight-unit chunks");

    namespace
    {
        constexpr float Pi = 3.141592654f;

        WorldPoint Rotate(WorldPoint p, float yaw)
        {
            return {std::cos(yaw) * p.x + std::sin(yaw) * p.y,
                    -std::sin(yaw) * p.x + std::cos(yaw) * p.y};
        }

        bool ClearProp(WorldPoint p, float extent)
        {
            // Test the entire footprint, not just its anchor, against protected paving.
            const auto minX = std::int64_t(std::floor(p.x - extent - .5));
            const auto maxX = std::int64_t(std::ceil(p.x + extent + .5));
            const auto minY = std::int64_t(std::floor(p.y - extent - .5));
            const auto maxY = std::int64_t(std::ceil(p.y + extent + .5));
            for (auto y = minY; y <= maxY; ++y)
            {
                for (auto x = minX; x <= maxX; ++x)
                {
                    if (LegacyTile(x, y))
                    {
                        return false;
                    }
                }
            }
            return true;
        }
    } // namespace

    const char* BiomeId(Biome biome)
    {
        static const char* ids[] = {"ash_grassland",
                                    "bell_mire",
                                    "dark_canopy_forest",
                                    "broken_crown_hills",
                                    "dead_furnace_wastes",
                                    "glass_ash_desert"};
        return ids[static_cast<size_t>(biome)];
    }

    std::int64_t FloorDiv(std::int64_t value, int divisor)
    {
        const auto quotient = value / divisor;
        return quotient - (value % divisor < 0 ? 1 : 0);
    }

    Biome RegionBiome(std::int64_t rx, std::int64_t ry, std::uint64_t seed)
    {
        const auto phase = [seed](int channel)
        {
            return (double(GameRules::Hash(channel, 0, seed) & 65535) / 65535 - .5) * .3;
        };
        // Each adjacent 64-unit region differs by at most one ecological step.
        // The sequence uses authored transitions and cannot put desert beside mire.
        const double value = 3.65 + std::sin(double(rx) * .75 - Pi / 2 + phase(0))
                             + .95 * std::cos(double(ry) * .75 + Pi + phase(1))
                             + .95 * std::sin((double(rx) + double(ry)) * .2 - Pi / 2 + phase(2));
        return static_cast<Biome>(std::clamp(int(std::floor(value)), 0, int(Biome::Count) - 1));
    }

    bool LegacyTile(std::int64_t x, std::int64_t y)
    {
        const double px = double(x);
        const double py = double(y);
        const double lx = px - GameRules::NearestShrine(px);
        const double ly = py - GameRules::NearestShrine(py);
        if (std::abs(lx) <= 1 || std::abs(ly) <= 1 || (std::abs(lx) < 6 && std::abs(ly) < 6))
        {
            return true;
        }
        for (const auto& village : Villages)
        {
            if (std::abs(px - village.center.x) <= GameRules::VillageHalfExtent + 8
                && std::abs(py - village.center.y) <= GameRules::VillageHalfExtent + 8)
            {
                return true;
            }
        }
        for (const auto& camp : Outposts)
        {
            if (std::abs(px - camp.center.x) <= OutpostHalfExtent + 4
                && std::abs(py - camp.center.y) <= OutpostHalfExtent + 4)
            {
                return true;
            }
        }
        return x >= GameRules::TutorialOffsetX + 2 && x <= GameRules::TutorialOffsetX + 24
               && y >= -5 && y <= 15;
    }

    Sampler::Sampler(const Catalog& catalog, std::uint64_t seed)
        : m_Catalog(catalog),
          m_Seed(seed)
    {
        std::cout << "[WorldChunks] generator=1 seed=" << seed
                  << " regionSide=64 chunkSide=8 planCacheLimit=1024 collision=shared-plan\n";
    }

    Sampler::Candidate Sampler::PlaceAt(std::int64_t rx, std::int64_t ry) const
    {
        Candidate candidate;
        const auto sample = GameRules::Hash(rx, ry, m_Seed ^ 0xAC79231ULL);
        if (sample % 100 >= 32)
        {
            return candidate;
        }
        const Biome biome = RegionBiome(rx, ry, m_Seed);
        std::vector<const Place*> choices;
        for (const auto& place : m_Catalog.places)
        {
            // Place floors are baked for a specific biome; do not recolor them blindly.
            if (place.biome == biome)
            {
                choices.push_back(&place);
            }
        }
        if (choices.empty())
        {
            return candidate;
        }
        candidate.x = rx * 8 + 2 + std::int64_t((sample >> 8) % 3);
        candidate.y = ry * 8 + 2 + std::int64_t((sample >> 16) % 3);
        for (int y = -1; y <= 16; ++y)
        {
            for (int x = -1; x <= 16; ++x)
            {
                if (LegacyTile(candidate.x * 8 + x, candidate.y * 8 + y))
                {
                    return {};
                }
            }
        }
        candidate.place = choices[(sample >> 24) % choices.size()];
        candidate.priority = sample;
        return candidate;
    }

    ChunkPlan Sampler::Generate(std::int64_t cx, std::int64_t cy) const
    {
        ChunkPlan plan;
        const auto rx = FloorDiv(cx, 8);
        const auto ry = FloorDiv(cy, 8);
        const int localX = int(cx - rx * 8);
        const int localY = int(cy - ry * 8);
        plan.biome = RegionBiome(rx, ry, m_Seed);
        const auto& bases = m_Catalog.bases.at(size_t(plan.biome));
        std::string key = bases.at(GameRules::Hash(cx, cy, m_Seed) % bases.size());

        // Two chunks straddle each region edge. Rotate the authored +X transition about its center.
        const auto transition = [&](Biome before, Biome after, int part, float yaw)
        {
            if (before == after)
            {
                return false;
            }
            auto found = m_Catalog.transitions.find({before, after});
            if (found == m_Catalog.transitions.end())
            {
                found = m_Catalog.transitions.find({after, before});
                part = 1 - part;
                yaw += Pi;
            }
            if (found == m_Catalog.transitions.end())
            {
                throw std::logic_error("Missing adjacent biome transition");
            }
            key = found->second.at(part);
            plan.yaw = yaw;
            const auto center = Rotate({3.5, 3.5}, yaw);
            plan.offset = {3.5 - center.x, 3.5 - center.y};
            plan.transition = true;
            return true;
        };
        bool selected = false;
        if (localX == 0)
        {
            selected = transition(RegionBiome(rx - 1, ry, m_Seed), plan.biome, 1, 0);
        }
        else if (localX == 7)
        {
            selected = transition(plan.biome, RegionBiome(rx + 1, ry, m_Seed), 0, 0);
        }
        if (!selected && localY == 0)
        {
            selected = transition(RegionBiome(rx, ry - 1, m_Seed), plan.biome, 1, -Pi / 2);
        }
        else if (!selected && localY == 7)
        {
            transition(plan.biome, RegionBiome(rx, ry + 1, m_Seed), 0, -Pi / 2);
        }

        auto place = PlaceAt(rx, ry);
        if (place.place && cx >= place.x && cx < place.x + 2 && cy >= place.y && cy < place.y + 2)
        {
            for (int y = -1; y <= 1; ++y)
            {
                for (int x = -1; x <= 1; ++x)
                {
                    if (x == 0 && y == 0)
                    {
                        continue;
                    }
                    const auto other = PlaceAt(rx + x, ry + y);
                    const bool priority =
                        other.priority > place.priority
                        || (other.priority == place.priority
                            && std::make_pair(other.x, other.y) > std::make_pair(place.x, place.y));
                    if (other.place && priority
                        && GameRules::Length(double(other.x - place.x) * 8,
                                             double(other.y - place.y) * 8)
                               < 64)
                    {
                        place.place = nullptr;
                    }
                }
            }
            if (place.place)
            {
                key = place.place->parts.at(size_t((cy - place.y) * 2 + cx - place.x));
                plan.placeId = std::to_string(m_Seed) + "/" + place.place->id + "/"
                               + std::to_string(place.x) + "/" + std::to_string(place.y) + "/"
                               + place.place->variant;
                plan.yaw = 0;
                plan.offset = {};
                plan.transition = false;
            }
        }
        plan.ground = m_Catalog.grounds.at(key);
        for (int y = 0; y < 8; ++y)
        {
            for (int x = 0; x < 8; ++x)
            {
                if (LegacyTile(cx * 8 + x, cy * 8 + y))
                {
                    plan.hiddenTiles |= std::uint64_t(1) << (y * 8 + x);
                }
            }
        }
        std::vector<Prop> props;
        bool hasCollider = false;
        for (auto prop : plan.ground.props)
        {
            const auto rotated = Rotate(prop.position, plan.yaw);
            prop.position = {rotated.x + plan.offset.x, rotated.y + plan.offset.y};
            prop.yaw += plan.yaw;
            const WorldPoint world{double(cx * 8) + prop.position.x,
                                   double(cy * 8) + prop.position.y};
            if (ClearProp(world, prop.extent) && !(prop.radius > 0 && hasCollider))
            {
                hasCollider |= prop.radius > 0;
                props.push_back(std::move(prop));
            }
        }
        plan.ground.props = std::move(props);
        return plan;
    }

    const ChunkPlan& Sampler::Chunk(std::int64_t cx, std::int64_t cy) const
    {
        const Key key{cx, cy};
        auto found = m_Cache.find(key);
        if (found != m_Cache.end())
        {
            ++m_Hits;
            m_Order.splice(m_Order.begin(), m_Order, found->second.order);
            return found->second.plan;
        }
        ++m_Misses;
        auto plan = Generate(cx, cy);
        if (m_Cache.size() >= 1024)
        {
            m_Cache.erase(m_Order.back());
            m_Order.pop_back();
        }
        m_Order.push_front(key);
        return m_Cache.emplace(key, Entry{std::move(plan), m_Order.begin()}).first->second.plan;
    }

    bool Sampler::Blocked(double x, double y) const
    {
        if (LegacyTile(std::int64_t(std::round(x)), std::int64_t(std::round(y))))
        {
            return false;
        }
        const auto cx = GameRules::ChunkOf(x);
        const auto cy = GameRules::ChunkOf(y);
        for (int oy = -1; oy <= 1; ++oy)
        {
            for (int ox = -1; ox <= 1; ++ox)
            {
                const auto& chunk = Chunk(cx + ox, cy + oy);
                for (const auto& prop : chunk.ground.props)
                {
                    if (prop.radius > 0
                        && GameRules::Length(x - double((cx + ox) * 8) - prop.position.x,
                                             y - double((cy + oy) * 8) - prop.position.y)
                               < prop.radius)
                    {
                        return true;
                    }
                }
            }
        }
        return false;
    }
} // namespace Terrain
