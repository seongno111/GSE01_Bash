#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"

#include <array>
#include <cmath>
#include <iostream>

bool Prototype::FindMonsterSpawn(WorldPoint desired, WorldPoint& result) const
{
    for (int ring = 0; ring <= 6; ++ring)
    {
        for (int y = -ring; y <= ring; ++y)
        {
            for (int x = -ring; x <= ring; ++x)
            {
                if (ring > 0 && std::abs(x) != ring && std::abs(y) != ring)
                {
                    continue;
                }
                const WorldPoint point{desired.x + x, desired.y + y};
                if (GameRules::InsideSanctuary(point.x, point.y) || Blocked(point.x - .2, point.y)
                    || Blocked(point.x + .2, point.y) || Blocked(point.x, point.y - .2)
                    || Blocked(point.x, point.y + .2))
                {
                    continue;
                }
                bool occupied = false;
                for (const auto& enemy : m_Enemies)
                {
                    if (GameRules::Length(point.x - enemy.homeX, point.y - enemy.homeY) < 1.1)
                    {
                        occupied = true;
                        break;
                    }
                }
                if (!occupied)
                {
                    result = point;
                    return true;
                }
            }
        }
    }
    return false;
}

void Prototype::InitializeMonsterEcology()
{
    struct Encounter
    {
        MonsterKind kind;
        WorldPoint home;
    };

    const std::array<Encounter, 5> encounters = {{{MonsterKind::AshWolf, {53, 2}},
                                                  {MonsterKind::AshWolf, {55, 4}},
                                                  {MonsterKind::AshWolf, {53, 6}},
                                                  {MonsterKind::BlightBear, {62, 12}},
                                                  {MonsterKind::ThornStag, {55, -12}}}};
    for (const auto& encounter : encounters)
    {
        WorldPoint home;
        if (!FindMonsterSpawn(encounter.home, home))
        {
            std::cerr << "[Monster] no walkable encounter spawn species="
                      << MonsterStats(encounter.kind).id << '\n';
            continue;
        }
        auto& enemy = SpawnEnemy(encounter.kind);
        enemy.homeX = home.x;
        enemy.homeY = home.y;
        enemy.SetWorldPosition(home.x, home.y);
        std::cout << "[Monster] spawn species=" << enemy.Definition().id << " home=" << home.x
                  << ',' << home.y << '\n';
    }
    std::cout << "[Monster] catalogSpecies=" << MonsterCatalog.size()
              << " newSpecies=6 proxyGeometry=cached\n";
}
