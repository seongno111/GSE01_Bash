#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"

#include <algorithm>
#include <cmath>

using namespace GameRules;

EnemyActor::EnemyActor(Prototype& game, int tier)
    : GameActor(game, "Enemy", TickGroup::Enemy, ActorLayer::World),
      tier(tier)
{
    maxHealth = tier == 0 ? 45.0f : tier == 1 ? 75.0f : 380.0f;
    health = maxHealth;
    damage = tier == 0 ? 8.0f : tier == 1 ? 12.0f : 20.0f;
    speed = tier == 0 ? 1.15f : tier == 1 ? 1.50f : 1.20f;
    experience = tier == 0 ? 20 : tier == 1 ? 30 : 80;
}

void EnemyActor::Update(float dt)
{
    auto& enemy = *this;
    const bool sanctuary = !m_Game.m_Player.EnabledInHierarchy()
                           || InsideSanctuary(m_Game.m_Player.WorldX(), m_Game.m_Player.WorldY());

    if (!enemy.alive)
    {
        enemy.respawn = (std::max)(0.0f, enemy.respawn - dt);
        if (enemy.respawn <= 0
            && Length(m_Game.m_Player.WorldX() - enemy.homeX,
                      m_Game.m_Player.WorldY() - enemy.homeY)
                   > 3.5)
        {
            enemy.SetWorldPosition(enemy.homeX, enemy.homeY);
            enemy.health = enemy.maxHealth;
            enemy.alive = true;
        }
        return;
    }

    double dx = m_Game.m_Player.WorldX() - enemy.WorldX();
    double dy = m_Game.m_Player.WorldY() - enemy.WorldY();
    const double distance = Length(dx, dy);
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
            if (!sanctuary && distance < 1.2 && m_Game.m_Player.hitCooldown <= 0)
            {
                m_Game.m_Player.health -= enemy.damage;
                m_Game.m_Player.hitCooldown = 0.8f;
                m_Game.AddCombatText(m_Game.m_Player.WorldX(),
                                     m_Game.m_Player.WorldY(),
                                     "-" + std::to_string(int(enemy.damage)),
                                     Color(1, 0.35f, 0.28f));
                if (m_Game.m_Player.health <= 0)
                {
                    m_Game.Die();
                    return;
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
                         returning ? enemy.homeX : m_Game.m_Player.WorldX(),
                         returning ? enemy.homeY : m_Game.m_Player.WorldY(),
                         speed,
                         dt);
    }
}

void EnemyActor::Draw(Renderer& r)
{
    if (alive)
    {
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
    const float height = enemy.tier == 2 ? 105.0f : 68.0f;
    const std::string name = enemy.tier == 0   ? "메마른 망자"
                             : enemy.tier == 1 ? "오염된 약탈자"
                                               : "경작지 수문장";
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
