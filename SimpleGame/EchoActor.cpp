#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"

#include <cmath>

using namespace GameRules;

EchoActor::EchoActor(Prototype& game,
                     std::uint64_t number,
                     double x,
                     double y,
                     int corruption,
                     float health,
                     bool alive,
                     float maxHealth,
                     float damage,
                     int level)
    : GameActor(game, "Echo", TickGroup::Enemy, ActorLayer::World),
      id(number),
      corruption(corruption),
      health(health),
      alive(alive),
      maxHealth(maxHealth),
      damage(damage),
      level(level)
{
    SetLocalPosition(x, y);
}

void EchoActor::Update(float dt)
{
    auto& echo = *this;
    const bool sanctuary = !m_Game.m_Player.EnabledInHierarchy()
                           || InsideSanctuary(m_Game.m_Player.WorldX(), m_Game.m_Player.WorldY());

    if (!echo.alive)
    {
        return;
    }

    const double dx = m_Game.m_Player.WorldX() - echo.WorldX();
    const double dy = m_Game.m_Player.WorldY() - echo.WorldY();
    const double distance = Length(dx, dy);
    if (distance > 0.7 && distance < 12 && !sanctuary)
    {
        m_Game.MoveActor(echo, dx / distance * 2.0 * dt, dy / distance * 2.0 * dt, 0.18f);
    }
    if (distance < 1.0 && !sanctuary && m_Game.m_Player.hitCooldown <= 0)
    {
        m_Game.m_Player.health -= echo.damage;
        m_Game.m_Player.hitCooldown = 1;
        if (m_Game.m_Player.health <= 0)
        {
            m_Game.Die();
            return;
        }
    }
}

void EchoActor::Draw(Renderer& r)
{
    const Point p = m_Game.Project(WorldX(), WorldY());
    if (DrawModel(
            r,
            "echo/death",
            [&]()
            {
                return m_Game.ModelAssets().Monster(4);
            },
            alive ? (SpritePose().moving ? "MON_04_Walk" : "MON_04_Idle") : "MON_04_DeadHold"))
    {
        return;
    }
    if (alive)
    {
        m_Game.DrawCharacter(r, p, corruption, true, SpritePose());
    }
    else
    {
        r.Ellipse(p, 16 * m_Game.m_Zoom, 7 * m_Game.m_Zoom, Color(0.28f, 0.10f, 0.25f, 0.75f));
    }
}

void EchoActor::DrawOverlay(Renderer& r)
{
    const auto& e = *this;
    const float w = float(m_Game.m_Width);
    const float h = float(m_Game.m_Height);

    if (!e.alive)
    {
        return;
    }
    const Point p = m_Game.Project(e.WorldX(), e.WorldY());
    if (p.x < -80 || p.x > w + 80 || p.y < 0 || p.y > h + 140)
    {
        return;
    }
    r.Rect(p.x - 25 * m_Game.m_Zoom,
           p.y - 89 * m_Game.m_Zoom,
           50 * m_Game.m_Zoom,
           3 * m_Game.m_Zoom,
           Ink);
    r.Rect(p.x - 25 * m_Game.m_Zoom,
           p.y - 89 * m_Game.m_Zoom,
           50 * m_Game.m_Zoom * e.health / e.maxHealth,
           3 * m_Game.m_Zoom,
           Violet);
    const std::string name = "잔재 " + std::to_string(e.id) + " · 레벨 " + std::to_string(e.level);
    r.Text(p.x - r.TextWidth(name, m_Game.m_Zoom) * 0.5f,
           p.y - 109 * m_Game.m_Zoom,
           name,
           Violet,
           m_Game.m_Zoom);
}
