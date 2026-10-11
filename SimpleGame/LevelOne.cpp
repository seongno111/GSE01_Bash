#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"
#include "NpcWorld.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <random>
#include <chrono>
#include <cstdlib>

using namespace GameRules;

int Prototype::LevelOneCell(std::int64_t x, std::int64_t y) const
{
    if (x < 4 || x > 22 || y < -3 || y > 13)
    {
        return -1;
    }

    return int((y + 3) * 19 + (x - 4));
}

void Prototype::GenerateLevelOneMap()
{
    std::mt19937_64 random(m_WorldSeed ^ 0xD7130AB5ULL);
    m_LevelWalkable.assign(19 * 17, false);
    const auto clear = [this](int x, int y)
    {
        const int cell = LevelOneCell(x, y);
        if (cell >= 0)
        {
            m_LevelWalkable[cell] = true;
        }
    };

    for (int y = -3; y <= 13; ++y)
    {
        for (int x = 4; x <= 22; ++x)
        {
            const bool perimeter = x == 4 || x == 22 || y == -3 || y == 13;
            m_LevelWalkable[LevelOneCell(x, y)] = perimeter || random() % 100 >= 24;
        }
    }

    // A guaranteed trunk connects the starting sanctuary and every encounter.
    for (int x = 4; x <= 22; ++x)
    {
        clear(x, 3);
        clear(x, 4);
    }
    for (const EnemyActor& enemy : m_Enemies)
    {
        const int homeX = int(std::round(enemy.homeX));
        const int homeY = int(std::round(enemy.homeY));
        for (int y = homeY - 1; y <= homeY + 1; ++y)
        {
            for (int x = homeX - 1; x <= homeX + 1; ++x)
            {
                clear(x, y);
            }
        }
        for (int y = (std::min)(3, homeY); y <= (std::max)(4, homeY); ++y)
        {
            clear(homeX, y);
            clear(homeX + 1, y);
        }
    }

    // Flood-fill before play: inaccessible pockets are filled, never used as rooms.
    std::vector<bool> reachable(m_LevelWalkable.size(), false);
    std::queue<std::pair<int, int>> pending;
    pending.push({4, 3});
    reachable[LevelOneCell(4, 3)] = true;
    const int dx[] = {1, -1, 0, 0};
    const int dy[] = {0, 0, 1, -1};
    while (!pending.empty())
    {
        const auto position = pending.front();
        pending.pop();
        for (int direction = 0; direction < 4; ++direction)
        {
            const int x = position.first + dx[direction];
            const int y = position.second + dy[direction];
            const int cell = LevelOneCell(x, y);
            if (cell >= 0 && m_LevelWalkable[cell] && !reachable[cell])
            {
                reachable[cell] = true;
                pending.push({x, y});
            }
        }
    }
    m_LevelWalkable = std::move(reachable);
}

void Prototype::InitializeLevelOne()
{
    if (m_LevelOneStarted)
    {
        return;
    }

    m_LevelOneStarted = true;
    m_WorldSeed =
        std::uint64_t(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    char* configuredSeed = nullptr;
    size_t configuredSeedSize = 0;
    if (_dupenv_s(&configuredSeed, &configuredSeedSize, "EMBER_WORLD_SEED") == 0
        && configuredSeed != nullptr)
    {
        char* end = nullptr;
        const auto value = std::strtoull(configuredSeed, &end, 10);
        if (end != configuredSeed && *end == '\0')
        {
            m_WorldSeed = value;
        }
    }
    std::free(configuredSeed);

    std::mt19937_64 random(m_WorldSeed);
    const auto spawn = [this, &random](double x, double y, int tier)
    {
        x += int(random() % 3) - 1;
        y += int(random() % 3) - 1;
        for (const EnemyActor& existing : m_Enemies)
        {
            if (Length(x - existing.homeX, y - existing.homeY) < 1.6)
            {
                x += 2;
            }
        }
        EnemyActor& enemy = SpawnEnemy(tier);
        enemy.SetWorldPosition(x, y);
        enemy.homeX = x;
        enemy.homeY = y;
        enemy.alive = tier != 2;
        enemy.SetEnabled(tier != 2);
        if (tier == 2)
        {
            m_GuardianId = enemy.Id();
        }
    };

    spawn(7, 1, 0);
    spawn(9, 3, 0);
    spawn(8, 6, 0);
    spawn(12, 1, 1);
    spawn(13, 5, 1);
    spawn(11, 9, 1);
    spawn(16, 6, 1);
    spawn(18, 2, 2);
    GenerateLevelOneMap();
    m_NpcWorld = std::make_unique<NpcWorld>(*this, m_WorldSeed);
}

void Prototype::CompleteLevelOne()
{
    if (m_LevelOneComplete || !m_GuardianDefeated
        || Length(m_Player.WorldX(), m_Player.WorldY()) >= 2.8)
    {
        return;
    }

    m_LevelOneComplete = true;
    m_Player.potions += 2;
    ++m_Player.relics;
    AddExperience(60);
    Notice(
        "레벨 1 완료! 성물 1개·회복약 2개·경험치 60 획득. U로 강화하며 계속 파밍할 수 있습니다.");
}
