#pragma once
#include "Renderer.h"
#include <cmath>
#include <cstdint>

namespace GameRules
{
    inline constexpr int ChunkSize = 8;
    inline constexpr int ShrineSpacing = 32;
    inline constexpr int MutationThreshold = 100;
    inline const Color Ink(0.025f, 0.035f, 0.042f);
    inline const Color Gold(0.84f, 0.65f, 0.36f);
    inline const Color Paper(0.78f, 0.79f, 0.73f);
    inline const Color Muted(0.43f, 0.52f, 0.51f);
    inline const Color Violet(0.65f, 0.30f, 0.65f);

    inline std::uint64_t Hash(std::int64_t x, std::int64_t y, std::uint64_t seed)
    {
        std::uint64_t n = std::uint64_t(x) * 0x9E3779B185EBCA87ULL
                          ^ std::uint64_t(y) * 0xC2B2AE3D27D4EB4FULL ^ seed;
        n ^= n >> 30;
        n *= 0xBF58476D1CE4E5B9ULL;
        n ^= n >> 27;
        n *= 0x94D049BB133111EBULL;
        return n ^ (n >> 31);
    }

    inline float Noise(std::int64_t x, std::int64_t y, std::uint64_t seed)
    {
        return float(Hash(x, y, seed) & 65535) / 65535.0f;
    }

    inline float SmoothNoise(double x, double y, std::uint64_t seed)
    {
        const auto ix = static_cast<std::int64_t>(std::floor(x));
        const auto iy = static_cast<std::int64_t>(std::floor(y));
        float fx = float(x - ix);
        float fy = float(y - iy);
        fx = fx * fx * (3 - 2 * fx);
        fy = fy * fy * (3 - 2 * fy);
        const float a = Noise(ix, iy, seed) * (1 - fx) + Noise(ix + 1, iy, seed) * fx;
        const float b = Noise(ix, iy + 1, seed) * (1 - fx) + Noise(ix + 1, iy + 1, seed) * fx;
        return a * (1 - fy) + b * fy;
    }

    inline double Distance(double x, double y)
    {
        return std::sqrt(x * x + y * y);
    }

    inline std::int64_t ChunkOf(double p)
    {
        return static_cast<std::int64_t>(std::floor(p / ChunkSize));
    }

    inline double NearestShrine(double p)
    {
        return std::round(p / ShrineSpacing) * ShrineSpacing;
    }

    inline constexpr int MaximumLevel = 50;
    inline constexpr float ProjectileSpeed = 11.0f;
    inline const Color ExperienceColor(0.43f, 0.79f, 0.68f);
    inline const Color DamageColor(1.0f, 0.79f, 0.42f);

    inline double Length(double x, double y)
    {
        return std::sqrt(x * x + y * y);
    }

    inline bool InsideSanctuary(double x, double y)
    {
        const double sx = std::round(x / 32.0) * 32.0;
        const double sy = std::round(y / 32.0) * 32.0;
        return Length(x - sx, y - sy) < 2.8;
    }
} // namespace GameRules
