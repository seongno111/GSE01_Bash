#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"
#include "NpcWorld.h"

#include <algorithm>
#include <cmath>

using namespace GameRules;

ProjectileActor::ProjectileActor(Prototype& game,
                                 double x,
                                 double y,
                                 double directionX,
                                 double directionY,
                                 float damage,
                                 float remaining,
                                 bool hostile)
    : GameActor(game, "Projectile", TickGroup::Projectile, ActorLayer::Effects),
      directionX(directionX),
      directionY(directionY),
      damage(damage),
      remaining(remaining),
      hostile(hostile)
{
    SetLocalPosition(x, y);
    allowPK =
        !hostile && m_Game.m_NpcWorld && m_Game.m_NpcWorld->PKEnabled() && !InsideSanctuary(x, y);
}

void ProjectileActor::Update(float dt)
{
    if (hostile)
    {
        auto& shot = *this;

        float movement = (std::min)(shot.remaining, 4.2f * dt);
        while (shot.alive && movement > 0)
        {
            const float step = (std::min)(movement, 0.08f);
            shot.TranslateWorld(shot.directionX * step, shot.directionY * step);
            shot.remaining -= step;
            movement -= step;
            if (m_Game.Blocked(shot.WorldX(), shot.WorldY()))
            {
                shot.alive = false;
            }
            else if (m_Game.m_NpcWorld
                     && m_Game.m_NpcWorld->HitByMonster(shot.WorldX(), shot.WorldY(), shot.damage))
            {
                shot.alive = false;
            }
            else if (m_Game.m_Player.EnabledInHierarchy()
                     && Length(shot.WorldX() - m_Game.m_Player.WorldX(),
                               shot.WorldY() - m_Game.m_Player.WorldY())
                            < 0.35)
            {
                shot.alive = false;
                if (m_Game.m_Player.hitCooldown <= 0
                    && !InsideSanctuary(m_Game.m_Player.WorldX(), m_Game.m_Player.WorldY()))
                {
                    m_Game.m_Player.health -= shot.damage;
                    m_Game.m_Player.hitCooldown = 0.8f;
                    m_Game.AddCombatText(m_Game.m_Player.WorldX(),
                                         m_Game.m_Player.WorldY(),
                                         "-" + std::to_string(int(shot.damage)),
                                         Color(1, 0.35f, 0.28f));
                    if (m_Game.m_Player.health <= 0)
                    {
                        m_Game.Die();
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
    else
    {
        auto& projectile = *this;

        float movement = (std::min)(projectile.remaining, ProjectileSpeed * dt);
        while (projectile.alive && movement > 0)
        {
            // Short collision steps prevent passing through props or small enemies.
            const float step = (std::min)(movement, 0.08f);
            projectile.TranslateWorld(projectile.directionX * step, projectile.directionY * step);
            projectile.remaining -= step;
            movement -= step;
            if (m_Game.Blocked(projectile.WorldX(), projectile.WorldY()))
            {
                projectile.alive = false;
                break;
            }

            for (EnemyActor& enemy : m_Game.m_Enemies)
            {
                const float radius = enemy.tier == 2 ? 0.58f : 0.40f;
                if (enemy.alive && enemy.EnabledInHierarchy()
                    && Length(projectile.WorldX() - enemy.WorldX(),
                              projectile.WorldY() - enemy.WorldY())
                           < radius)
                {
                    enemy.health -= projectile.damage;
                    projectile.alive = false;
                    m_Game.AddCombatText(enemy.WorldX(),
                                         enemy.WorldY(),
                                         std::to_string(int(projectile.damage)),
                                         DamageColor);
                    if (enemy.health <= 0)
                    {
                        m_Game.DefeatEnemy(enemy);
                    }
                    break;
                }
            }

            if (projectile.alive && m_Game.m_NpcWorld
                && m_Game.m_NpcWorld->HitByPlayer(projectile.WorldX(),
                                                  projectile.WorldY(),
                                                  projectile.damage,
                                                  allowPK))
            {
                projectile.alive = false;
            }

            if (!projectile.alive)
            {
                break;
            }

            for (EchoActor& echo : m_Game.m_Echoes)
            {
                if (echo.alive && echo.EnabledInHierarchy()
                    && Length(projectile.WorldX() - echo.WorldX(),
                              projectile.WorldY() - echo.WorldY())
                           < 0.45)
                {
                    echo.health -= projectile.damage;
                    projectile.alive = false;
                    m_Game.AddCombatText(echo.WorldX(),
                                         echo.WorldY(),
                                         std::to_string(int(projectile.damage)),
                                         DamageColor);
                    if (echo.health <= 0)
                    {
                        echo.alive = false;
                        if (m_Game.m_NpcWorld)
                        {
                            m_Game.m_NpcWorld->RecordWorldEvent("hunt",
                                                                "플레이어가 변이 잔재를 토벌했다",
                                                                echo.WorldPosition(),
                                                                &m_Game.m_Player,
                                                                &echo);
                        }
                        m_Game.Notice("잔재가 쓰러졌다. 그 흔적은 이 땅에 남는다.");
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
    if (!alive)
    {
        Destroy();
    }
}

void ProjectileActor::Draw(Renderer& r)
{
    if (hostile)
    {
        const auto& shot = *this;

        Point p = m_Game.Project(shot.WorldX(), shot.WorldY());
        p.y -= 39 * m_Game.m_Zoom;
        r.Ellipse(p, 5 * m_Game.m_Zoom, 5 * m_Game.m_Zoom, Color(1, 0.19f, 0.12f).Radiance(4));
    }
    else
    {
        const auto& projectile = *this;

        Point head = m_Game.Project(projectile.WorldX(), projectile.WorldY());
        Point tail = m_Game.Project(projectile.WorldX() - projectile.directionX * 0.35,
                                    projectile.WorldY() - projectile.directionY * 0.35);
        head.y -= 39 * m_Game.m_Zoom;
        tail.y -= 39 * m_Game.m_Zoom;
        r.Line(tail, head, 3 * m_Game.m_Zoom, Color(1.0f, 0.62f, 0.24f).Radiance(5));
        r.Ellipse(head,
                  3 * m_Game.m_Zoom,
                  3 * m_Game.m_Zoom,
                  Color(1.0f, 0.88f, 0.59f).Radiance(7));
    }
}
