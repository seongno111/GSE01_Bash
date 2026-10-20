#pragma once
#include "Actor.h"
#include "Villages.h"
#include <array>

struct OutpostDefinition
{
    const char* id;
    const char* name;
    WorldPoint center;
};

inline constexpr std::array<OutpostDefinition, 5> Outposts = {
    {{"ash", "잿더미 야영지", {74, -32}},
     {"crypt", "무너진 납골지", {-74, -32}},
     {"thorns", "가시숲 둥지", {-74, 74}},
     {"quarry", "오염된 채석장", {74, 74}},
     {"fang", "붉은 이빨 군락", {180, 74}}}};

inline constexpr int OutpostHalfExtent = 12;
inline constexpr int OutpostInitialPopulation = 12;
inline constexpr int OutpostCapacity = 36;
inline constexpr int OutpostWaveThreshold = 24;
inline constexpr int OutpostWaveSize = 16;
inline constexpr float OutpostGarrisonRadius = 16;
inline constexpr float OutpostInitialPatrolRadius = 18;
inline constexpr float OutpostMaximumPatrolRadius = 48;
inline constexpr double OutpostSpawnInterval = 8;
static_assert(OutpostInitialPopulation <= OutpostCapacity,
              "Initial outpost population must fit its capacity");
static_assert(OutpostWaveSize > 0 && OutpostWaveSize <= OutpostWaveThreshold
                  && OutpostWaveThreshold <= OutpostCapacity,
              "Outpost wave limits must fit the garrison");
