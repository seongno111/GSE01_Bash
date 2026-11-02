#pragma once
#include "Actor.h"
#include "ModelLibrary.h"

#include <array>
#include <list>
#include <map>

namespace Terrain
{
    enum class Biome
    {
        Grassland,
        Mire,
        Forest,
        Hills,
        Wastes,
        Desert,
        Count
    };

    const char* BiomeId(Biome biome);
    Biome RegionBiome(std::int64_t rx, std::int64_t ry, std::uint64_t seed);
    std::int64_t FloorDiv(std::int64_t value, int divisor);
    bool LegacyTile(std::int64_t x, std::int64_t y);

    struct Prop
    {
        Models::ModelInstance model;
        std::string key;
        WorldPoint position;
        float yaw = 0;
        float elevation = 0;
        float radius = 0;
        float extent = 0;
        bool canopy = false;
    };

    struct Ground
    {
        Models::ModelInstance model;
        std::string key;
        std::vector<Prop> props;
    };

    struct Place
    {
        std::string id;
        std::string variant;
        Biome biome = Biome::Grassland;
        std::array<std::string, 4> parts;
    };

    struct Catalog
    {
        std::map<std::string, Ground> grounds;
        std::array<std::vector<std::string>, size_t(Biome::Count)> bases;
        std::map<std::pair<Biome, Biome>, std::array<std::string, 2>> transitions;
        std::vector<Place> places;
    };

    struct ChunkPlan
    {
        Ground ground;
        WorldPoint offset;
        float yaw = 0;
        std::uint64_t hiddenTiles = 0;
        Biome biome = Biome::Grassland;
        std::string placeId;
        bool transition = false;
    };

    // Bounded CPU-only plans are shared by streaming and distant collision queries.
    class Sampler
    {
    public:

        Sampler(const Catalog& catalog, std::uint64_t seed);
        const ChunkPlan& Chunk(std::int64_t cx, std::int64_t cy) const;
        bool Blocked(double x, double y) const;

        size_t CacheSize() const
        {
            return m_Cache.size();
        }

        size_t CacheHits() const
        {
            return m_Hits;
        }

        size_t CacheMisses() const
        {
            return m_Misses;
        }

    private:

        using Key = std::pair<std::int64_t, std::int64_t>;

        struct Entry
        {
            ChunkPlan plan;
            std::list<Key>::iterator order;
        };

        struct Candidate
        {
            const Place* place = nullptr;
            std::int64_t x = 0;
            std::int64_t y = 0;
            std::uint64_t priority = 0;
        };

        Candidate PlaceAt(std::int64_t rx, std::int64_t ry) const;
        ChunkPlan Generate(std::int64_t cx, std::int64_t cy) const;
        const Catalog& m_Catalog;
        std::uint64_t m_Seed;
        mutable std::map<Key, Entry> m_Cache;
        mutable std::list<Key> m_Order;
        mutable size_t m_Hits = 0;
        mutable size_t m_Misses = 0;
    };
} // namespace Terrain
