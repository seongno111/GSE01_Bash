#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"
#include "NpcWorld.h"

#include <algorithm>
#include <cmath>

using namespace GameRules;

EnemyActor::EnemyActor(Prototype& game, int tier)
    : EnemyActor(game,
                 tier == 2   ? MonsterKind::FieldWarden
                 : tier == 1 ? MonsterKind::TaintedRaider
                             : MonsterKind::WitheredHollow)
{
}

EnemyActor::EnemyActor(Prototype& game, MonsterKind species)
    : GameActor(game, MonsterStats(species).name, TickGroup::Enemy, ActorLayer::World),
      kind(species)
{
    const auto& stats = Definition();
    tier = kind == MonsterKind::FieldWarden ? 2 : kind == MonsterKind::TaintedRaider ? 1 : 0;
    maxHealth = stats.health;
    health = maxHealth;
    damage = stats.damage;
    speed = stats.moveSpeed;
    experience = stats.experience;
    specialCooldown = stats.specialCooldown * .5f;
}

void EnemyActor::Update(float dt)
{
    if (outpost >= 0)
    {
        return;
    }
    auto& enemy = *this;
    Actor* target = &m_Game.m_Player;
    if (m_Game.m_NpcWorld)
    {
        if (!CommittingAttack())
        {
            if (m_Game.Time() >= nextTargetScan)
            {
                nextTargetScan = m_Game.Time() + .4f;
                target = m_Game.m_NpcWorld->EnemyTarget(*this);
                combatTarget = target ? target->Id() : InvalidActor;
            }
            else
            {
                target = m_Game.Scene().Find(combatTarget);
            }
        }
        else
        {
            target = m_Game.Scene().Find(combatTarget);
        }
    }
    const auto* npc = dynamic_cast<const NpcActor*>(target);
    const bool sanctuary = !target || !target->EnabledInHierarchy() || (npc && !npc->vitals.alive)
                           || InsideSanctuary(target->WorldX(), target->WorldY());
    const double targetX = target ? target->WorldX() : homeX;
    const double targetY = target ? target->WorldY() : homeY;

    if (!enemy.alive)
    {
        enemy.respawn = (std::max)(0.0f, enemy.respawn - dt);
        if (enemy.respawn <= 0
            && Length(m_Game.m_Player.WorldX() - enemy.homeX,
                      m_Game.m_Player.WorldY() - enemy.homeY)
                   > 3.5)
        {
            enemy.SetWorldPosition(enemy.homeX, enemy.homeY);
            enemy.ResetAnimation();
            enemy.ResetCombat();
            enemy.health = enemy.maxHealth;
            enemy.alive = true;
        }
        return;
    }

    double dx = targetX - enemy.WorldX();
    double dy = targetY - enemy.WorldY();
    const double distance = Length(dx, dy);
    if (UsesMonsterTactics())
    {
        const bool returnHome =
            (target && sanctuary)
            || Length(WorldX() - homeX, WorldY() - homeY) > Definition().leashRange;
        if (!UpdateMonsterTactics(dt, sanctuary ? nullptr : target, returnHome))
        {
            WorldPoint destination{homeX, homeY};
            if (!returnHome)
            {
                const double phase =
                    double(Hash(Id(), std::int64_t(m_Game.Time() / 8), m_Game.m_WorldSeed) % 6283)
                    * .001;
                destination = {homeX + std::cos(phase) * 2.5, homeY + std::sin(phase) * 2.5};
            }
            if (Length(WorldX() - destination.x, WorldY() - destination.y) > .5)
            {
                m_Game.MoveEnemy(*this, destination.x, destination.y, speed * .65f, dt);
            }
        }
        return;
    }
    const bool returning =
        sanctuary || distance > 8
        || Length(enemy.WorldX() - enemy.homeX, enemy.WorldY() - enemy.homeY) > 7;
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
                    m_Game.m_EnemyProjectiles.Spawn(m_Game,
                                                    enemy.WorldX(),
                                                    enemy.WorldY(),
                                                    std::cos(angle),
                                                    std::sin(angle),
                                                    enraged ? 18.0f : 14.0f,
                                                    8.0f,
                                                    true);
                }
                enemy.burstCooldown = enraged ? 3.0f : 4.5f;
            }
            return;
        }
        else if (enemy.burstCooldown <= 0 && distance < 8)
        {
            enemy.burstWindup = 0.9f;
            return;
        }
    }
    enemy.recovery = (std::max)(0.0f, enemy.recovery - dt);
    if (enemy.attackWindup > 0)
    {
        enemy.attackWindup = (std::max)(0.0f, enemy.attackWindup - dt);
        if (enemy.attackWindup <= 0)
        {
            enemy.recovery = 1.15f;
            if (!sanctuary && distance < 1.2 && target
                && m_Game.ClearShot(WorldX(), WorldY(), target->WorldX(), target->WorldY()))
            {
                if (m_Game.m_NpcWorld)
                {
                    m_Game.m_NpcWorld->MonsterDamage(*target, enemy.damage);
                }
                else if (m_Game.m_Player.hitCooldown <= 0)
                {
                    m_Game.m_Player.health -= enemy.damage;
                    m_Game.m_Player.hitCooldown = 0.8f;
                    if (m_Game.m_Player.health <= 0)
                    {
                        m_Game.Die();
                    }
                }
            }
        }
        return;
    }

    if (returning)
    {
        dx = enemy.homeX - enemy.WorldX();
        dy = enemy.homeY - enemy.WorldY();
    }
    else if (distance < 0.90 && enemy.recovery <= 0)
    {
        enemy.attackWindup = enemy.tier == 2 ? 0.65f : 0.5f;
        return;
    }

    const double travel = Length(dx, dy);
    const double aggro = enemy.tier == 0 ? 4.5 : 6.5;
    if (travel > 0.7 && (returning || distance < aggro) && enemy.recovery <= 0)
    {
        const float speed = enemy.speed * (enraged ? 1.3f : 1.0f);
        m_Game.MoveEnemy(enemy,
                         returning ? enemy.homeX : targetX,
                         returning ? enemy.homeY : targetY,
                         speed,
                         dt);
    }
}

void EnemyActor::Draw(Renderer& r)
{
    if (alive)
    {
        if (UsesMonsterTactics())
        {
            m_Game.DrawMonsterIndicators(r, m_Game.Project(WorldX(), WorldY()), *this);
            if (DrawSpeciesModel(r))
            {
                return;
            }
        }
        if (!UsesMonsterTactics())
        {
            const int model = kind == MonsterKind::WitheredHollow  ? 0
                              : kind == MonsterKind::TaintedRaider ? 1
                                                                   : 2;
            const std::string prefix = "MON_0" + std::to_string(model) + "_";
            const std::string clip = burstWindup > 0                    ? prefix + "Burst"
                                     : attackWindup > 0 || recovery > 0 ? prefix + "Attack"
                                     : SpritePose().moving              ? prefix + "Walk"
                                                                        : prefix + "Idle";
            float progress = -1;
            if (attackWindup > 0)
            {
                progress = (Definition().attackWindup - attackWindup)
                           / (Definition().attackWindup + Definition().attackRecovery);
            }
            else if (recovery > 0)
            {
                progress = 1 - recovery / (Definition().attackWindup + Definition().attackRecovery);
            }
            if (DrawModel(
                    r,
                    prefix,
                    [&]()
                    {
                        return m_Game.ModelAssets().Monster(model);
                    },
                    clip,
                    progress))
            {
                return;
            }
        }
        m_Game.DrawEnemyMesh(r, m_Game.Project(WorldX(), WorldY()), *this);
    }
}

void EnemyActor::DrawOverlay(Renderer& r)
{
    const auto& enemy = *this;
    const Color text(0.82f, 0.84f, 0.78f);
    const Color panel(0.025f, 0.035f, 0.042f, 0.91f);

    if (!enemy.alive)
    {
        return;
    }

    Point p = m_Game.Project(enemy.WorldX(), enemy.WorldY());
    if (p.x < -80 || p.x > m_Game.m_Width + 80 || p.y < 0 || p.y > m_Game.m_Height + 130)
    {
        return;
    }
    const auto* model = m_Game.ModelAssets().MonsterSpecies(enemy.Definition().modelKey);
    const float height =
        (std::max)(enemy.Definition().visualHeight, model ? model->height * 40 : 0.0f) + 24;
    const std::string name = enemy.Definition().name;
    r.Text(p.x - r.TextWidth(name, 1.0f) * 0.5f, p.y - height * m_Game.m_Zoom, name, text, 1.0f);
    r.Rect(p.x - 24 * m_Game.m_Zoom,
           p.y - (height - 17) * m_Game.m_Zoom,
           48 * m_Game.m_Zoom,
           4 * m_Game.m_Zoom,
           panel);
    r.Rect(p.x - 24 * m_Game.m_Zoom,
           p.y - (height - 17) * m_Game.m_Zoom,
           48 * m_Game.m_Zoom * (enemy.health / enemy.maxHealth),
           4 * m_Game.m_Zoom,
           Color(0.72f, 0.33f, 0.20f));
}
