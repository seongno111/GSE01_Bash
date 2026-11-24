#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"
#include "HeroWeapons.h"
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
    if (distance > m_Player.range
        || !ClearShot(m_Player.WorldX(), m_Player.WorldY(), targetX, targetY))
    {
        return;
    }

    const double directionX = distance > 0.0001 ? dx / distance : 1.0;
    const double directionY = distance > 0.0001 ? dy / distance : 0.0;
    m_Player.weaponAttack = nullptr;
    const bool mutant = m_Player.corruption >= MutationThreshold;
    if (!mutant || m_ModelAssets.CorruptedHero(HeroWeapons[m_Player.weapon].key))
    {
        if (const auto* visual = m_ModelAssets.HeroWeaponActions(HeroWeapons[m_Player.weapon].key))
        {
            m_Player.weaponAttack =
                &visual->attacks[m_Player.nextAttack++ % visual->attacks.size()];
        }
    }
    m_Player.shotCooldown = ProjectileCooldown();
    m_Player.weaponAttackStart = m_Time;
    m_Player.weaponAttackDuration = m_Player.shotCooldown;
    m_Player.attackTarget = m_TargetId;
    m_Player.attackWeapon = m_Player.weapon;
    m_Player.attackMutant = mutant;
    m_Player.attackDamage = ProjectileDamage();
    const auto* action = m_Player.weaponAttack;
    const float contact = action ? action->contact / action->duration : .45f;
    m_Player.attackReleaseAt = m_Time + m_Player.weaponAttackDuration * contact;
    m_Player.BeginAttackAnimation(directionX, directionY, m_Player.weaponAttackDuration, true);
}

void Prototype::ResolvePlayerAttack()
{
    // Simulation owns the contact event. Render frequency and visibility cannot duplicate hits.
    m_Player.attackReleaseAt = -1;
    if (m_Player.health <= 0 || m_Player.attackWeapon != m_Player.weapon
        || m_Player.attackMutant != (m_Player.corruption >= MutationThreshold))
    {
        return;
    }
    const auto& weapon = HeroWeapons[m_Player.weapon];
    m_FireVfx.WeaponContact(weapon.skill.kind,
                            m_Player.WorldPosition(),
                            m_Player.ModelYaw(),
                            weapon.range,
                            weapon.skill.width,
                            false);
    double targetX = 0;
    double targetY = 0;
    if (m_Player.attackTarget == InvalidActor
        || !TargetPosition(targetX, targetY, m_Player.attackTarget))
    {
        return;
    }
    const double dx = targetX - m_Player.WorldX();
    const double dy = targetY - m_Player.WorldY();
    const double distance = Length(dx, dy);
    if (distance > m_Player.range
        || !ClearShot(m_Player.WorldX(), m_Player.WorldY(), targetX, targetY))
    {
        return;
    }
    const double directionX = distance > .0001 ? dx / distance : 1.0;
    const double directionY = distance > .0001 ? dy / distance : 0.0;
    m_Player.attack = .16f;
    if (!HeroWeapons[m_Player.weapon].ranged)
    {
        if (auto* target = m_Scene.Find(m_Player.attackTarget))
        {
            ApplyPlayerHit(*target, m_Player.attackDamage);
        }
    }
    else
    {
        auto& projectile = m_Projectiles.Spawn(*this,
                                               m_Player.WorldX(),
                                               m_Player.WorldY(),
                                               directionX,
                                               directionY,
                                               m_Player.attackDamage,
                                               m_Player.range);
        projectile.arrow = m_Player.weapon == 1;
    }
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
    const auto from = enemy.WorldPosition();
    if (dt <= 0 || Length(targetX - from.x, targetY - from.y) < .05)
    {
        CancelRoute(enemy.Id());
        return;
    }

    // Follow the cached detour even if the center line to the goal becomes visible.
    // Replacing it every tick can pull the monster back into the same corner.
    const bool changed = Length(enemy.routeTarget.x - targetX, enemy.routeTarget.y - targetY) > .6;
    if (m_Time >= enemy.nextRoute && (changed || enemy.routeIndex >= enemy.route.size()))
    {
        const WorldPoint target{targetX, targetY};
        const auto result = RequestRoute(enemy, target, enemy.route);
        if (result != RouteStatus::Pending)
        {
            enemy.routeIndex = 0;
            enemy.routeTarget = target;
            enemy.nextRoute = m_Time + (result == RouteStatus::Found ? .35 : 1.0);
        }
    }

    while (enemy.routeIndex < enemy.route.size())
    {
        const auto next = enemy.route[enemy.routeIndex];
        const double dx = next.x - from.x;
        const double dy = next.y - from.y;
        const double distance = Length(dx, dy);
        if (distance < .001)
        {
            ++enemy.routeIndex;
            continue;
        }

        const double fraction = (std::min)(1.0, double(speed * dt) / distance);
        const WorldPoint position{from.x + dx * fraction, from.y + dy * fraction};
        // Use the same straight segment as pathfinding. Axis sliding here
        // would take the monster off the segment and back into the obstacle.
        if (!ClearMovement(from, position, .18))
        {
            CancelRoute(enemy.Id());
            enemy.route.clear();
            enemy.routeIndex = 0;
            enemy.nextRoute = (std::max)(enemy.nextRoute, m_Time + .35);
            return;
        }

        enemy.SetWorldPosition(position.x, position.y);
        enemy.RecordMovement(position.x - from.x, position.y - from.y);
        if (fraction == 1)
        {
            ++enemy.routeIndex;
        }
        return;
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

bool Prototype::TargetPosition(double& x, double& y, ActorId target) const
{
    const ::Actor* actor = m_Scene.Find(target == InvalidActor ? m_TargetId : target);
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
