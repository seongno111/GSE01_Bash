#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"

#include <algorithm>
#include <cmath>

using namespace GameRules;

LootActor::LootActor(Prototype& game, double x, double y, int amount, LootType type)
    : GameActor(game, "Loot", TickGroup::Loot, ActorLayer::Effects),
      amount(amount),
      type(type)
{
    SetLocalPosition(x, y);
}

void LootActor::Update(float dt)
{
    auto& loot = *this;

    loot.remaining -= dt;
    if (!m_Game.m_Player.EnabledInHierarchy())
    {
        if (remaining <= 0)
        {
            Destroy();
        }
        return;
    }
    const double dx = m_Game.m_Player.WorldX() - loot.WorldX();
    const double dy = m_Game.m_Player.WorldY() - loot.WorldY();
    const double distance = Length(dx, dy);
    if (distance < 0.65 && loot.amount > 0
        && m_Game.ClearShot(loot.WorldX(),
                            loot.WorldY(),
                            m_Game.m_Player.WorldX(),
                            m_Game.m_Player.WorldY()))
    {
        m_Game.CollectLoot(loot);
        loot.amount = 0;
    }
    else if (distance > 0.001 && distance < (m_Game.m_Player.magnetTime > 0 ? 5.5 : 2.4)
             && m_Game.ClearShot(loot.WorldX(),
                                 loot.WorldY(),
                                 m_Game.m_Player.WorldX(),
                                 m_Game.m_Player.WorldY()))
    {
        const double step = (std::min)(distance, 5.0 * dt);
        loot.TranslateWorld(dx / distance * step, dy / distance * step);
    }

    if (amount == 0 || remaining <= 0)
    {
        Destroy();
    }
}

void LootActor::Draw(Renderer& r)
{
    const auto& loot = *this;

    const Point p = m_Game.Project(loot.WorldX(), loot.WorldY());
    const float z = m_Game.m_Zoom;
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
