#include "stdafx.h"
#include "Prototype.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <random>
#include <chrono>
#include <queue>

namespace
{
    constexpr int MaximumLevel = 50;
    constexpr float ProjectileSpeed = 11.0f;
    const Color ExperienceColor(0.43f, 0.79f, 0.68f);
    const Color DamageColor(1.0f, 0.79f, 0.42f);

    double Length(double x, double y)
    {
        return std::sqrt(x * x + y * y);
    }

    bool InsideSanctuary(double x, double y)
    {
        const double sx = std::round(x / 32.0) * 32.0;
        const double sy = std::round(y / 32.0) * 32.0;
        return Length(x - sx, y - sy) < 2.8;
    }
} // namespace

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
    for (const FarmEnemy& enemy : m_FarmEnemies)
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

void Prototype::CollectLoot(Loot& loot)
{
    ++m_CollectedItems;
    switch (loot.type)
    {
    case LootType::Upgrade:
        m_Shards += loot.amount;
        AddCombatText(m_X, m_Y, "강화 파편 +" + std::to_string(loot.amount), DamageColor);
        break;
    case LootType::Healing:
        m_Health = (std::min)(MaxHealth(), m_Health + MaxHealth() * 0.35f);
        AddCombatText(m_X, m_Y, "생명력 회복", Color(0.95f, 0.40f, 0.38f));
        break;
    case LootType::Essence:
        AddExperience(loot.amount);
        AddCombatText(m_X,
                      m_Y,
                      "불의 정수 +" + std::to_string(loot.amount) + " 경험치",
                      DamageColor);
        break;
    case LootType::Purification:
        m_PurificationStones += loot.amount;
        AddCombatText(m_X, m_Y, "정화석 +1 [T]", Color(0.48f, 0.81f, 1));
        break;
    case LootType::Magnet:
        m_MagnetTime = 15;
        AddCombatText(m_X, m_Y, "자석 효과 15초", ExperienceColor);
        break;
    }
}

float Prototype::MaxHealth() const
{
    return 100.0f + 15.0f * (m_Level - 1);
}

float Prototype::ProjectileDamage() const
{
    return 24.0f + 5.0f * (m_Level - 1) + 4.0f * m_WeaponRank + 0.3f * m_Corruption;
}

float Prototype::ProjectileCooldown() const
{
    return (std::max)(0.22f, 0.90f * std::pow(0.94f, float(m_Level - 1)));
}

int Prototype::ExperienceRequired() const
{
    return 40 + 25 * (m_Level - 1);
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
    std::mt19937_64 random(m_WorldSeed);
    const auto spawn = [this, &random](double x, double y, int tier)
    {
        x += int(random() % 3) - 1;
        y += int(random() % 3) - 1;
        for (const FarmEnemy& existing : m_FarmEnemies)
        {
            if (Length(x - existing.homeX, y - existing.homeY) < 1.6)
            {
                x += 2;
            }
        }
        FarmEnemy enemy{};
        enemy.x = x;
        enemy.y = y;
        enemy.homeX = x;
        enemy.homeY = y;
        enemy.tier = tier;
        enemy.maxHealth = tier == 0 ? 45.0f : tier == 1 ? 75.0f : 380.0f;
        enemy.health = enemy.maxHealth;
        enemy.damage = tier == 0 ? 8.0f : tier == 1 ? 12.0f : 20.0f;
        enemy.speed = tier == 0 ? 1.15f : tier == 1 ? 1.50f : 1.20f;
        enemy.experience = tier == 0 ? 20 : tier == 1 ? 30 : 80;
        enemy.alive = tier != 2;
        m_FarmEnemies.push_back(enemy);
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
}

bool Prototype::ClearShot(double fromX, double fromY, double toX, double toY) const
{
    const double dx = toX - fromX;
    const double dy = toY - fromY;
    const int steps = (std::max)(1, int(std::ceil(Length(dx, dy) / 0.12)));

    for (int i = 1; i <= steps; ++i)
    {
        const double fraction = double(i) / steps;
        if (Blocked(fromX + dx * fraction, fromY + dy * fraction))
        {
            return false;
        }
    }

    return true;
}

void Prototype::SelectTarget()
{
    m_TargetKind = 0;
    double nearest = m_Range + 0.0001;
    const auto consider = [this, &nearest](double x, double y, int kind, size_t index)
    {
        const double distance = Length(x - m_X, y - m_Y);
        if (distance <= m_Range && distance < nearest && ClearShot(m_X, m_Y, x, y))
        {
            nearest = distance;
            m_TargetKind = kind;
            m_TargetIndex = index;
        }
    };

    for (size_t i = 0; i < m_FarmEnemies.size(); ++i)
    {
        const FarmEnemy& enemy = m_FarmEnemies[i];
        if (enemy.alive)
        {
            consider(enemy.x, enemy.y, 1, i);
        }
    }

    for (size_t i = 0; i < m_Echoes.size(); ++i)
    {
        const Echo& echo = m_Echoes[i];
        if (echo.alive)
        {
            consider(echo.x, echo.y, 2, i);
        }
    }
}

bool Prototype::TargetPosition(double& x, double& y) const
{
    if (m_TargetKind == 1 && m_TargetIndex < m_FarmEnemies.size()
        && m_FarmEnemies[m_TargetIndex].alive)
    {
        x = m_FarmEnemies[m_TargetIndex].x;
        y = m_FarmEnemies[m_TargetIndex].y;
        return true;
    }

    if (m_TargetKind == 2 && m_TargetIndex < m_Echoes.size() && m_Echoes[m_TargetIndex].alive)
    {
        x = m_Echoes[m_TargetIndex].x;
        y = m_Echoes[m_TargetIndex].y;
        return true;
    }

    return false;
}

void Prototype::FireProjectile()
{
    double targetX = 0;
    double targetY = 0;
    if (!TargetPosition(targetX, targetY))
    {
        return;
    }

    const double dx = targetX - m_X;
    const double dy = targetY - m_Y;
    const double distance = Length(dx, dy);
    if (distance > m_Range)
    {
        return;
    }

    const double directionX = distance > 0.0001 ? dx / distance : 1.0;
    const double directionY = distance > 0.0001 ? dy / distance : 0.0;
    m_Projectiles.push_back({m_X, m_Y, directionX, directionY, ProjectileDamage(), m_Range});
    m_ShotCooldown = ProjectileCooldown();
    m_Attack = 0.16f;
}

void Prototype::AddCombatText(double x, double y, const std::string& text, Color color)
{
    m_CombatText.push_back({x, y, text, color});
}

void Prototype::AddExperience(int amount)
{
    if (m_Level >= MaximumLevel)
    {
        return;
    }

    m_Experience += amount;
    while (m_Level < MaximumLevel && m_Experience >= ExperienceRequired())
    {
        m_Experience -= ExperienceRequired();
        ++m_Level;
        m_Health = (std::min)(MaxHealth(), m_Health + 15.0f + MaxHealth() * 0.20f);
        m_ShotCooldown = (std::min)(m_ShotCooldown, ProjectileCooldown());
        Notice("레벨 " + std::to_string(m_Level)
               + " 달성! 최대 체력·발사체 피해 증가, 발사 대기시간 감소.");
        AddCombatText(m_X, m_Y, "레벨 상승!", ExperienceColor);
    }

    if (m_Level == MaximumLevel)
    {
        m_Experience = 0;
    }
}

void Prototype::DefeatEnemy(FarmEnemy& enemy)
{
    if (!enemy.alive)
    {
        return;
    }

    enemy.alive = false;
    enemy.respawn = enemy.tier == 2 ? 45.0f : 25.0f;
    enemy.attackWindup = 0;
    enemy.recovery = 0;
    enemy.burstWindup = 0;
    enemy.burstCooldown = 5;
    ++m_FarmKills;
    const size_t firstDrop = m_Loot.size();
    m_Loot.push_back({enemy.x - 0.2, enemy.y, enemy.tier == 2 ? 3 : 1, LootType::Upgrade});
    const auto bonus = static_cast<LootType>(1 + (m_FarmKills - 1) % 4);
    m_Loot.push_back({enemy.x + 0.3, enemy.y, bonus == LootType::Essence ? 25 : 1, bonus});
    if (enemy.tier == 2)
    {
        m_Loot.push_back({enemy.x, enemy.y + 0.4, 80, LootType::Essence});
        m_Loot.push_back({enemy.x, enemy.y - 0.4, 1, LootType::Purification});
    }
    for (size_t i = firstDrop; i < m_Loot.size(); ++i)
    {
        if (Blocked(m_Loot[i].x, m_Loot[i].y))
        {
            m_Loot[i].x = enemy.x;
            m_Loot[i].y = enemy.y;
        }
    }
    AddExperience(enemy.experience);
    AddCombatText(enemy.x, enemy.y, "경험치 +" + std::to_string(enemy.experience), ExperienceColor);

    if (enemy.tier == 2)
    {
        m_GuardianDefeated = true;
        Notice("보스 경작지 수문장을 처치했다! 전리품을 챙겨 시작 성소에서 E로 보고하세요.");
    }
}

void Prototype::UpdateProjectiles(float dt)
{
    for (Projectile& projectile : m_Projectiles)
    {
        float movement = (std::min)(projectile.remaining, ProjectileSpeed * dt);
        while (projectile.alive && movement > 0)
        {
            // Short collision steps prevent passing through props or small enemies.
            const float step = (std::min)(movement, 0.08f);
            projectile.x += projectile.directionX * step;
            projectile.y += projectile.directionY * step;
            projectile.remaining -= step;
            movement -= step;
            if (Blocked(projectile.x, projectile.y))
            {
                projectile.alive = false;
                break;
            }

            for (FarmEnemy& enemy : m_FarmEnemies)
            {
                const float radius = enemy.tier == 2 ? 0.58f : 0.40f;
                if (enemy.alive && Length(projectile.x - enemy.x, projectile.y - enemy.y) < radius)
                {
                    enemy.health -= projectile.damage;
                    projectile.alive = false;
                    AddCombatText(enemy.x,
                                  enemy.y,
                                  std::to_string(int(projectile.damage)),
                                  DamageColor);
                    if (enemy.health <= 0)
                    {
                        DefeatEnemy(enemy);
                    }
                    break;
                }
            }

            if (!projectile.alive)
            {
                break;
            }

            for (Echo& echo : m_Echoes)
            {
                if (echo.alive && Length(projectile.x - echo.x, projectile.y - echo.y) < 0.45)
                {
                    echo.health -= projectile.damage;
                    projectile.alive = false;
                    AddCombatText(echo.x,
                                  echo.y,
                                  std::to_string(int(projectile.damage)),
                                  DamageColor);
                    if (echo.health <= 0)
                    {
                        echo.alive = false;
                        Notice("잔재가 쓰러졌다. 그 흔적은 이 땅에 남는다.");
                    }
                    break;
                }
            }
        }

        if (projectile.remaining <= 0)
        {
            projectile.alive = false;
        }
    }

    m_Projectiles.erase(std::remove_if(m_Projectiles.begin(),
                                       m_Projectiles.end(),
                                       [](const Projectile& projectile)
                                       {
                                           return !projectile.alive;
                                       }),
                        m_Projectiles.end());

    for (Projectile& shot : m_EnemyProjectiles)
    {
        float movement = (std::min)(shot.remaining, 4.2f * dt);
        while (shot.alive && movement > 0)
        {
            const float step = (std::min)(movement, 0.08f);
            shot.x += shot.directionX * step;
            shot.y += shot.directionY * step;
            shot.remaining -= step;
            movement -= step;
            if (Blocked(shot.x, shot.y))
            {
                shot.alive = false;
            }
            else if (Length(shot.x - m_X, shot.y - m_Y) < 0.35)
            {
                shot.alive = false;
                if (m_HitCooldown <= 0 && !InsideSanctuary(m_X, m_Y))
                {
                    m_Health -= shot.damage;
                    m_HitCooldown = 0.8f;
                    AddCombatText(m_X,
                                  m_Y,
                                  "-" + std::to_string(int(shot.damage)),
                                  Color(1, 0.35f, 0.28f));
                    if (m_Health <= 0)
                    {
                        Die();
                        return;
                    }
                }
            }
        }
        if (shot.remaining <= 0)
        {
            shot.alive = false;
        }
    }
    m_EnemyProjectiles.erase(std::remove_if(m_EnemyProjectiles.begin(),
                                            m_EnemyProjectiles.end(),
                                            [](const Projectile& shot)
                                            {
                                                return !shot.alive;
                                            }),
                             m_EnemyProjectiles.end());
}

void Prototype::MoveFarmEnemy(FarmEnemy& enemy,
                              double targetX,
                              double targetY,
                              float speed,
                              float dt)
{
    double dx = targetX - enemy.x;
    double dy = targetY - enemy.y;
    double distance = Length(dx, dy);
    if (distance < 0.05)
    {
        return;
    }
    if (ClearShot(enemy.x, enemy.y, targetX, targetY))
    {
        const double previousX = enemy.x;
        const double previousY = enemy.y;
        const double step = (std::min)(distance, double(speed * dt));
        Move(enemy.x, enemy.y, dx / distance * step, dy / distance * step, 0.18f);
        if (Length(enemy.x - previousX, enemy.y - previousY) > 0.001)
        {
            return;
        }
    }

    const int start =
        LevelOneCell(std::int64_t(std::round(enemy.x)), std::int64_t(std::round(enemy.y)));
    const int goalX = std::clamp(int(std::round(targetX)), 4, 22);
    const int goalY = std::clamp(int(std::round(targetY)), -3, 13);
    const int goal = LevelOneCell(goalX, goalY);
    if (start < 0 || !m_LevelWalkable[start] || !m_LevelWalkable[goal] || start == goal)
    {
        return;
    }

    std::vector<int> previous(m_LevelWalkable.size(), -1);
    std::queue<int> pending;
    previous[start] = start;
    pending.push(start);
    const int offsetsX[] = {1, -1, 0, 0};
    const int offsetsY[] = {0, 0, 1, -1};
    while (!pending.empty() && previous[goal] < 0)
    {
        const int cell = pending.front();
        pending.pop();
        const int x = cell % 19 + 4;
        const int y = cell / 19 - 3;
        for (int direction = 0; direction < 4; ++direction)
        {
            const int next = LevelOneCell(x + offsetsX[direction], y + offsetsY[direction]);
            if (next >= 0 && m_LevelWalkable[next] && previous[next] < 0)
            {
                previous[next] = cell;
                pending.push(next);
            }
        }
    }
    if (previous[goal] < 0)
    {
        return;
    }

    int next = goal;
    while (previous[next] != start)
    {
        next = previous[next];
    }
    dx = next % 19 + 4 - enemy.x;
    dy = next / 19 - 3 - enemy.y;
    distance = Length(dx, dy);
    if (distance > 0.001)
    {
        const double step = (std::min)(distance, double(speed * dt));
        Move(enemy.x, enemy.y, dx / distance * step, dy / distance * step, 0.18f);
    }
}

void Prototype::UpdateEnemies(float dt)
{
    const bool sanctuary = InsideSanctuary(m_X, m_Y);
    const bool guardianUnlocked = m_Level >= 3 && m_FarmKills >= 6 && m_CollectedItems >= 4;
    if (guardianUnlocked && !m_GuardianAnnounced)
    {
        m_GuardianAnnounced = true;
        Notice("보스 출현! 수문장은 충격파를 발사합니다. 붉은 예고가 보이면 거리를 벌리세요.");
    }

    for (FarmEnemy& enemy : m_FarmEnemies)
    {
        if (enemy.tier == 2 && !guardianUnlocked)
        {
            continue;
        }

        if (!enemy.alive)
        {
            enemy.respawn = (std::max)(0.0f, enemy.respawn - dt);
            if (enemy.respawn <= 0 && Length(m_X - enemy.homeX, m_Y - enemy.homeY) > 3.5)
            {
                enemy.x = enemy.homeX;
                enemy.y = enemy.homeY;
                enemy.health = enemy.maxHealth;
                enemy.alive = true;
            }
            continue;
        }

        double dx = m_X - enemy.x;
        double dy = m_Y - enemy.y;
        const double distance = Length(dx, dy);
        const bool returning =
            sanctuary || distance > 8 || Length(enemy.x - enemy.homeX, enemy.y - enemy.homeY) > 7;
        const bool enraged = enemy.tier == 2 && enemy.health <= enemy.maxHealth * 0.4f;
        if (enemy.tier == 2)
        {
            enemy.burstCooldown = (std::max)(0.0f, enemy.burstCooldown - dt);
            if (returning)
            {
                enemy.burstWindup = 0;
            }
            else if (enemy.burstWindup > 0)
            {
                enemy.burstWindup = (std::max)(0.0f, enemy.burstWindup - dt);
                if (enemy.burstWindup <= 0)
                {
                    const int count = enraged ? 12 : 8;
                    const double startAngle = std::atan2(dy, dx);
                    for (int i = 0; i < count; ++i)
                    {
                        const double angle = startAngle + i * 6.283185307 / count;
                        m_EnemyProjectiles.push_back({enemy.x,
                                                      enemy.y,
                                                      std::cos(angle),
                                                      std::sin(angle),
                                                      enraged ? 18.0f : 14.0f,
                                                      8.0f});
                    }
                    enemy.burstCooldown = enraged ? 3.0f : 4.5f;
                }
                continue;
            }
            else if (enemy.burstCooldown <= 0 && distance < 8)
            {
                enemy.burstWindup = 0.9f;
                continue;
            }
        }
        enemy.recovery = (std::max)(0.0f, enemy.recovery - dt);
        if (enemy.attackWindup > 0)
        {
            enemy.attackWindup = (std::max)(0.0f, enemy.attackWindup - dt);
            if (enemy.attackWindup <= 0)
            {
                enemy.recovery = 1.15f;
                if (!sanctuary && distance < 1.2 && m_HitCooldown <= 0)
                {
                    m_Health -= enemy.damage;
                    m_HitCooldown = 0.8f;
                    AddCombatText(m_X,
                                  m_Y,
                                  "-" + std::to_string(int(enemy.damage)),
                                  Color(1, 0.35f, 0.28f));
                    if (m_Health <= 0)
                    {
                        Die();
                        return;
                    }
                }
            }
            continue;
        }

        if (returning)
        {
            dx = enemy.homeX - enemy.x;
            dy = enemy.homeY - enemy.y;
        }
        else if (distance < 0.90 && enemy.recovery <= 0)
        {
            enemy.attackWindup = enemy.tier == 2 ? 0.65f : 0.5f;
            continue;
        }

        const double travel = Length(dx, dy);
        const double aggro = enemy.tier == 0 ? 4.5 : 6.5;
        if (travel > 0.7 && (returning || distance < aggro) && enemy.recovery <= 0)
        {
            const float speed = enemy.speed * (enraged ? 1.3f : 1.0f);
            MoveFarmEnemy(enemy,
                          returning ? enemy.homeX : m_X,
                          returning ? enemy.homeY : m_Y,
                          speed,
                          dt);
        }
    }

    // Keep echo rewards separate: deliberate deaths cannot manufacture farm XP.
    bool killed = false;
    for (Echo& echo : m_Echoes)
    {
        if (!echo.alive)
        {
            continue;
        }

        const double dx = m_X - echo.x;
        const double dy = m_Y - echo.y;
        const double distance = Length(dx, dy);
        if (distance > 0.7 && distance < 12 && !sanctuary)
        {
            Move(echo.x, echo.y, dx / distance * 2.0 * dt, dy / distance * 2.0 * dt, 0.18f);
        }
        if (distance < 1.0 && !sanctuary && m_HitCooldown <= 0)
        {
            m_Health -= echo.damage;
            m_HitCooldown = 1;
            if (m_Health <= 0)
            {
                killed = true;
                break;
            }
        }
    }

    if (killed)
    {
        Die();
    }
}

void Prototype::UpdateLevelOne(float dt)
{
    InitializeLevelOne();
    m_MagnetTime = (std::max)(0.0f, m_MagnetTime - dt);
    m_ShotCooldown = (std::max)(0.0f, m_ShotCooldown - dt);
    SelectTarget();
    if ((m_AutoFire || m_Keys[' ']) && m_ShotCooldown <= 0)
    {
        FireProjectile();
    }

    UpdateProjectiles(dt);
    UpdateEnemies(dt);

    for (Loot& loot : m_Loot)
    {
        loot.remaining -= dt;
        const double dx = m_X - loot.x;
        const double dy = m_Y - loot.y;
        const double distance = Length(dx, dy);
        if (distance < 0.65 && loot.amount > 0 && ClearShot(loot.x, loot.y, m_X, m_Y))
        {
            CollectLoot(loot);
            loot.amount = 0;
        }
        else if (distance > 0.001 && distance < (m_MagnetTime > 0 ? 5.5 : 2.4)
                 && ClearShot(loot.x, loot.y, m_X, m_Y))
        {
            const double step = (std::min)(distance, 5.0 * dt);
            loot.x += dx / distance * step;
            loot.y += dy / distance * step;
        }
    }

    m_Loot.erase(std::remove_if(m_Loot.begin(),
                                m_Loot.end(),
                                [](const Loot& loot)
                                {
                                    return loot.amount == 0 || loot.remaining <= 0;
                                }),
                 m_Loot.end());

    for (CombatText& text : m_CombatText)
    {
        text.remaining -= dt;
    }
    m_CombatText.erase(std::remove_if(m_CombatText.begin(),
                                      m_CombatText.end(),
                                      [](const CombatText& text)
                                      {
                                          return text.remaining <= 0;
                                      }),
                       m_CombatText.end());

    SelectTarget();
}

void Prototype::CompleteLevelOne()
{
    if (m_LevelOneComplete || !m_GuardianDefeated || Length(m_X, m_Y) >= 2.8)
    {
        return;
    }

    m_LevelOneComplete = true;
    m_Potions += 2;
    ++m_Relics;
    AddExperience(60);
    Notice(
        "레벨 1 완료! 성물 1개·회복약 2개·경험치 60 획득. U로 강화하며 계속 파밍할 수 있습니다.");
}

void Prototype::DrawFarmEnemy(Renderer& r, Point p, const FarmEnemy& enemy)
{
    const float z = m_Zoom * (enemy.tier == 2 ? 1.45f : 0.92f);
    const Color body = enemy.tier == 0   ? Color(0.32f, 0.40f, 0.29f)
                       : enemy.tier == 1 ? Color(0.38f, 0.28f, 0.24f)
                                         : Color(0.36f, 0.19f, 0.22f);
    r.Ellipse(p, 19 * z, 8 * z, Color(0.015f, 0.02f, 0.02f, 0.6f));
    if (enemy.attackWindup > 0)
    {
        r.Ellipse(p, 39 * z, 19 * z, Color(0.95f, 0.18f, 0.09f, 0.25f));
    }
    if (enemy.burstWindup > 0)
    {
        r.Ellipse(p, 90 * z, 45 * z, Color(0.95f, 0.12f, 0.09f, 0.15f));
        r.Ellipse({p.x, p.y - 32 * z}, 7 * z, 7 * z, Color(1, 0.19f, 0.08f).Radiance(4));
    }
    r.DrawCachedMesh(
        "farm-enemy/" + std::to_string(enemy.tier),
        p,
        z,
        [&]()
        {
            r.Line({p.x - 6 * z, p.y}, {p.x - 8 * z, p.y - 17 * z}, 5 * z, body.Shade(0.6f));
            r.Line({p.x + 6 * z, p.y}, {p.x + 8 * z, p.y - 17 * z}, 5 * z, body.Shade(0.6f));
            r.Triangle({p.x - 17 * z, p.y - 9 * z},
                       {p.x + 16 * z, p.y - 9 * z},
                       {p.x, p.y - 41 * z},
                       body);
            r.Ellipse({p.x, p.y - 35 * z}, 10 * z, 12 * z, body.Shade(1.3f));
            r.Rect(p.x - 6 * z, p.y - 38 * z, 12 * z, 5 * z, Color(0.035f, 0.04f, 0.035f));
            r.Rect(p.x - 4 * z, p.y - 37 * z, 2 * z, 2 * z, Color(0.82f, 0.88f, 0.4f).Radiance(2));
            r.Rect(p.x + 3 * z, p.y - 37 * z, 2 * z, 2 * z, Color(0.82f, 0.88f, 0.4f).Radiance(2));
            r.Line({p.x + 12 * z, p.y - 23 * z}, {p.x + 23 * z, p.y - 8 * z}, 4 * z, body);
            if (enemy.tier == 2)
            {
                r.Line({p.x - 14 * z, p.y - 23 * z},
                       {p.x - 23 * z, p.y + 2 * z},
                       5 * z,
                       Color(0.50f, 0.48f, 0.36f));
                r.Triangle({p.x - 9 * z, p.y - 42 * z},
                           {p.x - 15 * z, p.y - 57 * z},
                           {p.x - 5 * z, p.y - 43 * z},
                           body);
                r.Triangle({p.x + 9 * z, p.y - 42 * z},
                           {p.x + 15 * z, p.y - 57 * z},
                           {p.x + 5 * z, p.y - 43 * z},
                           body);
            }
        });
}

void Prototype::DrawCombatEffects(Renderer& r)
{
    if (m_AutoFire || m_Keys[' '])
    {
        for (int i = 0; i < 64; ++i)
        {
            const double angle = i * 6.283185307 / 64;
            const Point p =
                Project(m_X + std::cos(angle) * m_Range, m_Y + std::sin(angle) * m_Range);
            r.Rect(p.x, p.y, 2, 2, Color(0.76f, 0.65f, 0.38f, 0.35f));
        }
    }
    for (const Loot& loot : m_Loot)
    {
        const Point p = Project(loot.x, loot.y);
        const float z = m_Zoom;
        const Color colors[] = {Color(0.88f, 0.72f, 0.36f),
                                Color(0.96f, 0.32f, 0.28f),
                                Color(1, 0.56f, 0.14f),
                                Color(0.44f, 0.77f, 1),
                                Color(0.47f, 0.95f, 0.63f)};
        const Color color = colors[static_cast<int>(loot.type)].Radiance(2);
        r.Quad({p.x, p.y - 12 * z},
               {p.x + 5 * z, p.y - 6 * z},
               {p.x, p.y},
               {p.x - 5 * z, p.y - 6 * z},
               color);
        if (loot.type == LootType::Healing)
        {
            r.Line({p.x - 3 * z, p.y - 6 * z}, {p.x + 3 * z, p.y - 6 * z}, z, Color(1, 1, 1));
            r.Line({p.x, p.y - 9 * z}, {p.x, p.y - 3 * z}, z, Color(1, 1, 1));
        }
    }

    for (const Projectile& projectile : m_Projectiles)
    {
        Point head = Project(projectile.x, projectile.y);
        Point tail = Project(projectile.x - projectile.directionX * 0.35,
                             projectile.y - projectile.directionY * 0.35);
        head.y -= 24 * m_Zoom;
        tail.y -= 24 * m_Zoom;
        r.Line(tail, head, 3 * m_Zoom, Color(1.0f, 0.62f, 0.24f).Radiance(5));
        r.Ellipse(head, 3 * m_Zoom, 3 * m_Zoom, Color(1.0f, 0.88f, 0.59f).Radiance(7));
    }
    for (const Projectile& shot : m_EnemyProjectiles)
    {
        Point p = Project(shot.x, shot.y);
        p.y -= 24 * m_Zoom;
        r.Ellipse(p, 5 * m_Zoom, 5 * m_Zoom, Color(1, 0.19f, 0.12f).Radiance(4));
    }
}

void Prototype::DrawLevelOneUI(Renderer& r)
{
    const Color text(0.82f, 0.84f, 0.78f);
    const Color panel(0.025f, 0.035f, 0.042f, 0.91f);
    for (const FarmEnemy& enemy : m_FarmEnemies)
    {
        if (!enemy.alive)
        {
            continue;
        }

        Point p = Project(enemy.x, enemy.y);
        if (p.x < -80 || p.x > m_Width + 80 || p.y < 0 || p.y > m_Height + 130)
        {
            continue;
        }
        const float height = enemy.tier == 2 ? 105.0f : 68.0f;
        const std::string name = enemy.tier == 0   ? "메마른 망자"
                                 : enemy.tier == 1 ? "오염된 약탈자"
                                                   : "경작지 수문장";
        r.Text(p.x - r.TextWidth(name, 1.0f) * 0.5f, p.y - height * m_Zoom, name, text, 1.0f);
        r.Rect(p.x - 24 * m_Zoom, p.y - (height - 17) * m_Zoom, 48 * m_Zoom, 4 * m_Zoom, panel);
        r.Rect(p.x - 24 * m_Zoom,
               p.y - (height - 17) * m_Zoom,
               48 * m_Zoom * (enemy.health / enemy.maxHealth),
               4 * m_Zoom,
               Color(0.72f, 0.33f, 0.20f));
    }

    double targetX = 0;
    double targetY = 0;
    if (TargetPosition(targetX, targetY))
    {
        const Point p = Project(targetX, targetY);
        for (int i = 0; i < 24; ++i)
        {
            const float angle = i * 6.2831853f / 24;
            const float next = (i + 1) * 6.2831853f / 24;
            r.Line({p.x + 25 * m_Zoom * std::cos(angle), p.y + 12 * m_Zoom * std::sin(angle)},
                   {p.x + 25 * m_Zoom * std::cos(next), p.y + 12 * m_Zoom * std::sin(next)},
                   m_Zoom,
                   DamageColor);
        }
    }

    for (const CombatText& message : m_CombatText)
    {
        Point p = Project(message.x, message.y);
        p.y -= 65 * m_Zoom + (1.1f - message.remaining) * 30;
        Color color = message.color;
        color.a = (std::min)(1.0f, message.remaining * 2);
        r.Text(p.x - r.TextWidth(message.text, 1.2f) * 0.5f, p.y, message.text, color, 1.2f);
    }

    // This compact objective panel remains visible when the help panels are hidden.
    std::string objective;
    double goalX = 8;
    double goalY = 3;
    if (m_LevelOneComplete)
    {
        objective = "입문 완료 · 자유 파밍 / 적은 일정 시간 뒤 재등장";
    }
    else if (m_FarmKills < 3)
    {
        objective = "1. SPACE로 사냥하기 " + std::to_string(m_FarmKills) + "/3";
    }
    else if (m_CollectedItems < 4)
    {
        objective = "2. 드랍 아이템에 접근해 자동 습득 " + std::to_string(m_CollectedItems) + "/4";
    }
    else if (m_Level < 3 || m_FarmKills < 6)
    {
        objective = "3. 레벨 3 / 적 6마리 처치 (현재 " + std::to_string(m_Level) + " / "
                    + std::to_string(m_FarmKills) + ")";
        goalX = 12;
        goalY = 5;
    }
    else if (!m_GuardianDefeated)
    {
        objective = "4. 보스 처치 · 충격파와 붉은 공격 예고를 피하세요";
        goalX = m_FarmEnemies.back().homeX;
        goalY = m_FarmEnemies.back().homeY;
    }
    else
    {
        objective = "5. 시작 성소로 귀환하여 E · 보스 처치 보고";
        goalX = 0;
        goalY = 0;
    }

    const float width = (std::min)(560.0f, float(m_Width) - 48);
    const float top = float(m_Height) - 213;
    r.Rect(24, top, width, 62, panel);
    const float scale =
        (std::max)(0.8f, (std::min)(1.25f, (width - 25) / (r.TextWidth(objective, 1.0f) + 1)));
    r.Text(36, top + 8, objective, text, scale);

    std::ostringstream combat;
    combat << (m_AutoFire ? "F 자동 사격 켜짐" : "SPACE 유지: 사격 / F 자동 사격")
           << " · 사거리 6 · " << (m_TargetKind ? "표적 포착" : "사거리 내 표적 없음");
    r.Text(36, top + 35, combat.str(), DamageColor, 1.0f);

    if (!m_LevelOneComplete)
    {
        const Point goal = Project(goalX, goalY);
        const Point player = Project(m_X, m_Y);
        const float dx = goal.x - player.x;
        const float dy = goal.y - player.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length > 60)
        {
            const Point end{player.x + dx / length * 49, player.y + dy / length * 49};
            const Point base{player.x + dx / length * 35, player.y + dy / length * 35};
            r.Triangle(end,
                       {base.x - dy / length * 5, base.y + dx / length * 5},
                       {base.x + dy / length * 5, base.y - dx / length * 5},
                       ExperienceColor);
        }
    }

    const float xpTop = float(m_Height) - 142;
    r.Rect(24, xpTop, width, 21, panel);
    const float fraction =
        m_Level >= MaximumLevel ? 1.0f : float(m_Experience) / ExperienceRequired();
    r.Rect(24, xpTop, width * fraction, 3, ExperienceColor);
    const std::string xp = "레벨 " + std::to_string(m_Level) + " · 경험치 "
                           + (m_Level >= MaximumLevel ? "최대 레벨"
                                                      : std::to_string(m_Experience) + " / "
                                                            + std::to_string(ExperienceRequired()));
    r.Text(36, xpTop + 5, xp, ExperienceColor, 1.0f);

    if (m_MagnetTime > 0)
    {
        r.Text(float(m_Width) - 280,
               float(m_Height) - 85,
               "자석 강화 " + std::to_string(int(std::ceil(m_MagnetTime))) + "초",
               ExperienceColor,
               1.2f);
    }

    if (!m_FarmEnemies.empty())
    {
        const FarmEnemy& boss = m_FarmEnemies.back();
        if (boss.alive && Length(m_X - boss.x, m_Y - boss.y) < 12)
        {
            const float bossWidth = (std::min)(440.0f, float(m_Width) * 0.36f);
            const float bossLeft = (float(m_Width) - bossWidth) * 0.5f;
            r.Rect(bossLeft, 111, bossWidth, 42, panel);
            const std::string title =
                boss.health <= boss.maxHealth * 0.4f ? "경작지 수문장 · 격노" : "경작지 수문장";
            r.Text(bossLeft + 10, 116, title, text, 1.25f);
            r.Rect(bossLeft + 10, 143, bossWidth - 20, 4, Color(0.18f, 0.12f, 0.13f));
            r.Rect(bossLeft + 10,
                   143,
                   (bossWidth - 20) * (boss.health / boss.maxHealth),
                   4,
                   Color(0.85f, 0.23f, 0.18f));
        }
    }
}
