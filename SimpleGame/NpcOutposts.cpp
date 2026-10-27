#include "stdafx.h"
#include "NpcWorld.h"
#include "Prototype.h"
#include "Outposts.h"
#include "GameRules.h"
#include <algorithm>
#include <cmath>
#include <iostream>

using namespace GameRules;

void NpcWorld::InitializeOutposts()
{
    m_Outposts.resize(Outposts.size());
    for (size_t index = 0; index < Outposts.size(); ++index)
    {
        auto& camp = m_Outposts[index];
        camp.nextSpawn = 12 + index * 2;
        camp.nextExpansion = 40 + index * 3;
        camp.radius = OutpostInitialPatrolRadius;
        for (int slot = 0; slot < OutpostCapacity; ++slot)
        {
            const int location = (slot * 7) % OutpostCapacity;
            const auto center = Outposts[index].center;
            WorldPoint home{center.x + (location % 6 - 2.5) * 3,
                            center.y + (location / 6 - 2.5) * 3};
            if (!m_Game.FindMonsterSpawn(home, home))
            {
                std::cerr << "[Monster] skipped blocked outpost slot=" << slot << " camp=" << index
                          << '\n';
                continue;
            }
            auto& enemy = m_Game.SpawnEnemy(OutpostMonster(index, slot));
            enemy.homeX = home.x;
            enemy.homeY = home.y;
            enemy.SetWorldPosition(enemy.homeX, enemy.homeY);
            enemy.outpost = int(index);
            enemy.alive = slot < OutpostInitialPopulation;
            camp.spawned += enemy.alive ? 1 : 0;
            enemy.SetEnabled(enemy.alive);
            enemy.nextTargetScan = index * .03 + slot * .07;
            camp.members.push_back(enemy.Id());
            m_Enemies.push_back(enemy.Id());
        }
        RecordWorldEvent("outpost",
                         std::string(Outposts[index].name) + " 형성: 초기 병력 "
                             + std::to_string(camp.spawned) + " / 정원 "
                             + std::to_string(OutpostCapacity),
                         Outposts[index].center);
    }
}

void NpcWorld::UpdateOutposts()
{
    for (size_t index = 0; index < m_Outposts.size(); ++index)
    {
        auto& camp = m_Outposts[index];
        const auto center = Outposts[index].center;
        if (m_Time >= camp.nextExpansion)
        {
            camp.nextExpansion = m_Time + 40;
            if (camp.radius < OutpostMaximumPatrolRadius)
            {
                camp.radius = (std::min)(OutpostMaximumPatrolRadius, camp.radius + 4);
                RecordWorldEvent("outpost_expansion",
                                 std::string(Outposts[index].name)
                                     + " 순찰·습격 반경 확대: " + std::to_string(int(camp.radius)),
                                 center);
            }
        }
        std::vector<EnemyActor*> garrison;
        EnemyActor* vacant = nullptr;
        for (auto id : camp.members)
        {
            auto* enemy = dynamic_cast<EnemyActor*>(m_Game.Scene().Find(id));
            if (!enemy)
            {
                continue;
            }
            enemy->patrolRadius = camp.radius;
            if (!enemy->alive && enemy->respawn <= 0 && !vacant)
            {
                vacant = enemy;
            }
            if (enemy->alive && enemy->raidVillage < 0 && !enemy->returningToOutpost
                && Length(enemy->WorldX() - center.x, enemy->WorldY() - center.y)
                       < OutpostGarrisonRadius)
            {
                garrison.push_back(enemy);
            }
        }
        if (m_Time >= camp.nextSpawn)
        {
            camp.nextSpawn = m_Time + OutpostSpawnInterval;
            if (vacant && Walkable(vacant->homeX, vacant->homeY))
            {
                vacant->SetWorldPosition(vacant->homeX, vacant->homeY);
                vacant->ResetAnimation();
                vacant->ResetCombat();
                vacant->health = vacant->maxHealth;
                vacant->alive = true;
                vacant->SetEnabled(true);
                vacant->raidVillage = -1;
                vacant->returningToOutpost = false;
                vacant->combatTarget = InvalidActor;
                vacant->attackWindup = 0;
                vacant->recovery = 0;
                vacant->route.clear();
                vacant->nextRoute = 0;
                ++camp.spawned;
                garrison.push_back(vacant);
                RecordWorldEvent("outpost_spawn",
                                 std::string(Outposts[index].name) + " 병력 증원",
                                 center,
                                 vacant);
            }
        }
        if (garrison.size() < OutpostWaveThreshold || m_Time < camp.nextWave)
        {
            continue;
        }
        int target = -1;
        double nearest = 1e9;
        for (size_t village = 0; village < Villages.size(); ++village)
        {
            bool inhabited = false;
            for (auto id : m_Actors)
            {
                const auto* npc = dynamic_cast<const NpcActor*>(m_Game.Scene().Find(id));
                if (npc && npc->vitals.alive && npc->profile.village == village)
                {
                    inhabited = true;
                    break;
                }
            }
            const auto destination = Villages[village].center;
            const double distance = Length(destination.x - center.x, destination.y - center.y);
            if (inhabited && distance < nearest)
            {
                target = int(village);
                nearest = distance;
            }
        }
        if (target < 0)
        {
            continue;
        }
        std::vector<EnemyActor*> raiders;
        for (auto* enemy : garrison)
        {
            if (enemy->kind != MonsterKind::HighDemon)
            {
                raiders.push_back(enemy);
            }
        }
        if (raiders.size() < OutpostWaveSize)
        {
            continue;
        }
        for (int member = 0; member < OutpostWaveSize; ++member)
        {
            auto& enemy = *raiders[member];
            enemy.raidVillage = target;
            enemy.returningToOutpost = false;
            enemy.raidUntil = m_Time
                              + JourneyBudget(enemy.WorldPosition(),
                                              Villages[size_t(target)].center,
                                              enemy.speed);
            enemy.route.clear();
            enemy.nextRoute = 0;
        }
        ++camp.waves;
        camp.nextWave = m_Time + 80;
        RecordWorldEvent("monster_wave",
                         std::string(Outposts[index].name) + " → " + Villages[target].name
                             + " 집단 습격: " + std::to_string(OutpostWaveSize) + "마리 / 웨이브 "
                             + std::to_string(camp.waves),
                         center,
                         raiders.front());
    }
}

bool NpcWorld::UpdateOutpostEnemy(EnemyActor& enemy, float dt)
{
    if (enemy.outpost < 0)
    {
        return false;
    }
    if (!enemy.alive)
    {
        enemy.respawn = (std::max)(0.0f, enemy.respawn - dt);
        enemy.raidVillage = -1;
        enemy.returningToOutpost = false;
        return true;
    }
    if (enemy.raidVillage >= 0
        && (m_Time >= enemy.raidUntil || enemy.health < enemy.maxHealth * .2f))
    {
        RecordWorldEvent("wave_retreat",
                         "습격 몬스터가 거점으로 철수한다",
                         enemy.WorldPosition(),
                         &enemy);
        enemy.raidVillage = -1;
        enemy.returningToOutpost = true;
        enemy.route.clear();
        enemy.nextRoute = 0;
    }
    const auto center = Outposts[size_t(enemy.outpost)].center;
    if (enemy.returningToOutpost
        && Length(enemy.WorldX() - enemy.homeX, enemy.WorldY() - enemy.homeY) < 1.2)
    {
        enemy.returningToOutpost = false;
    }
    const bool beyondLeash =
        enemy.raidVillage < 0
        && Length(enemy.WorldX() - center.x, enemy.WorldY() - center.y)
               > (enemy.kind == MonsterKind::HighDemon ? enemy.Definition().leashRange
                                                       : enemy.patrolRadius)
                     + 2;
    if (m_Time >= enemy.nextTargetScan && !enemy.CommittingAttack())
    {
        enemy.nextTargetScan = m_Time + .5;
        Actor* target = beyondLeash || enemy.returningToOutpost ? nullptr : EnemyTarget(enemy);
        enemy.combatTarget = target ? target->Id() : InvalidActor;
    }
    Actor* target = m_Game.Scene().Find(enemy.combatTarget);
    const auto* npc = dynamic_cast<const NpcActor*>(target);
    if (target
        && (!target->EnabledInHierarchy() || (npc && !npc->vitals.alive)
            || InsideSanctuary(target->WorldX(), target->WorldY()) || beyondLeash
            || enemy.returningToOutpost))
    {
        target = nullptr;
        enemy.combatTarget = InvalidActor;
    }
    const double distance =
        target ? Length(target->WorldX() - enemy.WorldX(), target->WorldY() - enemy.WorldY()) : 1e9;
    if (!target && enemy.raidVillage < 0
        && Length(enemy.WorldX() - enemy.homeX, enemy.WorldY() - enemy.homeY) < 3)
    {
        enemy.health = (std::min)(enemy.maxHealth, enemy.health + dt * 2);
    }
    if (enemy.UsesMonsterTactics())
    {
        if (enemy.UpdateMonsterTactics(dt, target, beyondLeash || enemy.returningToOutpost))
        {
            return true;
        }
    }
    else
    {
        enemy.recovery = (std::max)(0.0f, enemy.recovery - dt);
    }
    if (enemy.attackWindup > 0)
    {
        enemy.attackWindup = (std::max)(0.0f, enemy.attackWindup - dt);
        if (enemy.attackWindup <= 0)
        {
            enemy.recovery = 1.2f;
            if (target && distance < 1.2
                && m_Game.ClearShot(enemy.WorldX(),
                                    enemy.WorldY(),
                                    target->WorldX(),
                                    target->WorldY()))
            {
                Damage(Entity(enemy), Entity(*target), enemy.damage);
            }
        }
        return true;
    }
    if (target && distance < .9 && enemy.recovery <= 0)
    {
        enemy.attackWindup = .55f;
        return true;
    }
    WorldPoint destination{enemy.homeX, enemy.homeY};
    if (target)
    {
        destination = target->WorldPosition();
    }
    else if (enemy.raidVillage >= 0)
    {
        const auto town = Villages[size_t(enemy.raidVillage)].center;
        const double phase = double(enemy.Id() % 6) * 1.047197551;
        destination = {town.x - 4 + std::cos(phase) * 2, town.y + 4 + std::sin(phase) * 2};
    }
    else if (!beyondLeash && !enemy.returningToOutpost)
    {
        const double phase =
            double(Hash(enemy.Id(), std::int64_t(m_Time / 12), m_Seed) % 6283) * .001;
        const double radius = enemy.kind == MonsterKind::HighDemon ? 9.0
                              : enemy.Id() % 3 == 0                ? enemy.patrolRadius
                                                                   : 9.0;
        const WorldPoint patrol{center.x + std::cos(phase) * radius,
                                center.y + std::sin(phase) * radius};
        if (Walkable(patrol.x, patrol.y) && !InsideSanctuary(patrol.x, patrol.y))
        {
            destination = patrol;
        }
    }
    if (enemy.recovery > 0
        || Length(destination.x - enemy.WorldX(), destination.y - enemy.WorldY()) < .4)
    {
        return true;
    }
    if (m_Time >= enemy.nextRoute
        || (enemy.routeIndex >= enemy.route.size() && m_Time >= enemy.nextRoute - .5))
    {
        ++m_Performance.pathCalls;
        enemy.routeIndex = 0;
        enemy.routeTarget = destination;
        m_Game.FindRoute(enemy.WorldPosition(), destination, enemy.route, m_Performance.pathNodes);
        enemy.nextRoute = m_Time + (target ? 1.0 : 3.0);
    }
    if (enemy.routeIndex < enemy.route.size())
    {
        const auto point = enemy.route[enemy.routeIndex];
        const double dx = point.x - enemy.WorldX();
        const double dy = point.y - enemy.WorldY();
        const double travel = Length(dx, dy);
        if (travel < .15)
        {
            ++enemy.routeIndex;
        }
        else
        {
            const double step =
                (std::min)(travel,
                           double(enemy.speed * dt * (enemy.raidVillage >= 0 ? 1.25f : 1.0f)));
            m_Game.MoveActor(enemy, dx / travel * step, dy / travel * step, .18f);
        }
    }
    return true;
}
