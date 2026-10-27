#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"
#include "NpcWorld.h"

#include <algorithm>
#include <cmath>

using namespace GameRules;

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

void Prototype::FireProjectile()
{
    double targetX = 0;
    double targetY = 0;
    if (!TargetPosition(targetX, targetY))
    {
        return;
    }

    const double dx = targetX - m_Player.WorldX();
    const double dy = targetY - m_Player.WorldY();
    const double distance = Length(dx, dy);
    if (distance > m_Player.range)
    {
        return;
    }

    const double directionX = distance > 0.0001 ? dx / distance : 1.0;
    const double directionY = distance > 0.0001 ? dy / distance : 0.0;
    m_Projectiles.Spawn(*this,
                        m_Player.WorldX(),
                        m_Player.WorldY(),
                        directionX,
                        directionY,
                        ProjectileDamage(),
                        m_Player.range);
    m_Player.shotCooldown = ProjectileCooldown();
    m_Player.attack = 0.16f;
    const float animationDuration = std::clamp(m_Player.shotCooldown * 0.75f, 0.22f, 0.32f);
    m_Player.BeginAttackAnimation(directionX, directionY, animationDuration);
}

void Prototype::DefeatEnemy(EnemyActor& enemy, bool rewardPlayer)
{
    if (!enemy.alive)
    {
        return;
    }

    enemy.alive = false;
    enemy.ResetAnimation();
    if (rewardPlayer && m_NpcWorld)
    {
        m_NpcWorld->RecordWorldEvent("hunt",
                                     "플레이어가 몬스터를 토벌했다",
                                     enemy.WorldPosition(),
                                     &m_Player,
                                     &enemy);
    }
    enemy.respawn = enemy.Definition().respawnSeconds;
    enemy.ResetCombat();
    enemy.burstCooldown = 5;
    if (!rewardPlayer)
    {
        if (enemy.Id() == m_GuardianId)
        {
            m_GuardianDefeated = true;
        }
        return;
    }
    ++m_FarmKills;
    const auto drop = [this, &enemy](double offsetX, double offsetY, int amount, LootType type)
    {
        double x = enemy.WorldX() + offsetX;
        double y = enemy.WorldY() + offsetY;
        if (Blocked(x, y))
        {
            x = enemy.WorldX();
            y = enemy.WorldY();
        }
        m_Loot.Spawn(*this, x, y, amount, type);
    };
    drop(-0.2, 0, enemy.Definition().boss ? 3 : 1, LootType::Upgrade);
    const auto bonus = static_cast<LootType>(1 + (m_FarmKills - 1) % 4);
    drop(0.3, 0, bonus == LootType::Essence ? 25 : 1, bonus);
    if (enemy.Definition().boss)
    {
        drop(0, 0.4, 80, LootType::Essence);
        drop(0, -0.4, 1, LootType::Purification);
    }
    AddExperience(enemy.experience);
    AddCombatText(enemy.WorldX(),
                  enemy.WorldY(),
                  "경험치 +" + std::to_string(enemy.experience),
                  ExperienceColor);

    if (enemy.Id() == m_GuardianId)
    {
        m_GuardianDefeated = true;
        Notice("보스 경작지 수문장을 처치했다! 전리품을 챙겨 시작 성소에서 E로 보고하세요.");
    }
}

void Prototype::MoveEnemy(EnemyActor& enemy, double targetX, double targetY, float speed, float dt)
{
    double dx = targetX - enemy.WorldX();
    double dy = targetY - enemy.WorldY();
    double distance = Length(dx, dy);
    if (distance < 0.05)
    {
        return;
    }
    if (ClearShot(enemy.WorldX(), enemy.WorldY(), targetX, targetY))
    {
        const double previousX = enemy.WorldX();
        const double previousY = enemy.WorldY();
        const double step = (std::min)(distance, double(speed * dt));
        MoveActor(enemy, dx / distance * step, dy / distance * step, 0.18f);
        if (Length(enemy.WorldX() - previousX, enemy.WorldY() - previousY) > 0.001)
        {
            return;
        }
    }

    // Use the same bounded world navigation as NPCs/outposts. The old tutorial
    // grid clamps used untranslated x=4..22 and could index vector<bool> at -1
    // after the encounter area moved east during settlement expansion.
    const WorldPoint target{targetX, targetY};
    const bool changed = Length(enemy.routeTarget.x - targetX, enemy.routeTarget.y - targetY) > 2;
    if (m_Time >= enemy.nextRoute || (changed && m_Time >= enemy.nextRoute - .25))
    {
        enemy.route.clear();
        enemy.routeIndex = 0;
        enemy.routeTarget = target;
        ++m_UpdateStats.pathCalls;
        FindRoute(enemy.WorldPosition(), target, enemy.route, m_UpdateStats.pathNodes);
        enemy.nextRoute = m_Time + .75;
    }
    while (enemy.routeIndex < enemy.route.size()
           && Length(enemy.route[enemy.routeIndex].x - enemy.WorldX(),
                     enemy.route[enemy.routeIndex].y - enemy.WorldY())
                  < .18)
    {
        ++enemy.routeIndex;
    }
    if (enemy.routeIndex >= enemy.route.size())
    {
        return;
    }
    const auto next = enemy.route[enemy.routeIndex];
    dx = next.x - enemy.WorldX();
    dy = next.y - enemy.WorldY();
    distance = Length(dx, dy);
    if (distance > .001)
    {
        const double step = (std::min)(distance, double(speed * dt));
        MoveActor(enemy, dx / distance * step, dy / distance * step, .18f);
    }
}

void Prototype::SelectTarget()
{
    m_TargetId = InvalidActor;
    if (!m_Player.EnabledInHierarchy())
    {
        return;
    }
    double nearest = m_Player.range + 0.0001;
    const auto consider = [this, &nearest](const ::Actor& actor)
    {
        if (!actor.EnabledInHierarchy())
        {
            return;
        }
        const double distance =
            Length(actor.WorldX() - m_Player.WorldX(), actor.WorldY() - m_Player.WorldY());
        if (distance <= m_Player.range && distance < nearest
            && ClearShot(m_Player.WorldX(), m_Player.WorldY(), actor.WorldX(), actor.WorldY()))
        {
            nearest = distance;
            m_TargetId = actor.Id();
        }
    };
    for (const EnemyActor& enemy : m_Enemies)
    {
        if (enemy.alive)
        {
            consider(enemy);
        }
    }
    for (const EchoActor& echo : m_Echoes)
    {
        if (echo.alive)
        {
            consider(echo);
        }
    }
    if (m_NpcWorld)
    {
        for (auto id : m_NpcWorld->Actors())
        {
            const auto* npc = dynamic_cast<const NpcActor*>(m_Scene.Find(id));
            if (npc && m_NpcWorld->PlayerCanTarget(*npc))
            {
                consider(*npc);
            }
        }
    }
}

bool Prototype::TargetPosition(double& x, double& y) const
{
    const ::Actor* actor = m_Scene.Find(m_TargetId);
    const auto* enemy = dynamic_cast<const EnemyActor*>(actor);
    const auto* echo = dynamic_cast<const EchoActor*>(actor);
    const auto* npc = dynamic_cast<const NpcActor*>(actor);
    if (!actor || !actor->EnabledInHierarchy()
        || !((enemy && enemy->alive) || (echo && echo->alive)
             || (npc && m_NpcWorld && m_NpcWorld->PlayerCanTarget(*npc))))
    {
        return false;
    }
    x = actor->WorldX();
    y = actor->WorldY();
    return true;
}

void Prototype::UpdateGameplay(float dt)
{
    m_Scene.Update(dt, TickGroup::Projectile);
    const bool guardianUnlocked = m_Player.level >= 3 && m_FarmKills >= 6 && m_CollectedItems >= 4;
    if (guardianUnlocked)
    {
        if (auto* guardian = dynamic_cast<EnemyActor*>(m_Scene.Find(m_GuardianId)))
        {
            guardian->SetEnabled(true);
        }
    }
    if (guardianUnlocked && !m_GuardianAnnounced)
    {
        m_GuardianAnnounced = true;
        Notice("보스 출현! 수문장은 충격파를 발사합니다. 붉은 예고가 보이면 거리를 벌리세요.");
    }
    m_Scene.Update(dt, TickGroup::Enemy);
    m_Scene.Update(dt, TickGroup::Loot);
    m_Scene.Update(dt, TickGroup::Effect);
    SelectTarget();
}
