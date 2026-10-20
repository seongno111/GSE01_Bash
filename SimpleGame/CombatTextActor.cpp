#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"

#include <algorithm>
#include <utility>

using namespace GameRules;

CombatTextActor::CombatTextActor(Prototype& game, double x, double y, std::string text, Color color)
    : GameActor(game, "CombatText", TickGroup::Effect, ActorLayer::Overlay),
      text(std::move(text)),
      color(color)
{
    SetLocalPosition(x, y);
}

void CombatTextActor::Update(float dt)
{
    remaining -= dt;
    if (remaining <= 0)
    {
        Destroy();
    }
}

void CombatTextActor::Draw(Renderer& r)
{
    const auto& message = *this;

    Point p = m_Game.Project(message.WorldX(), message.WorldY());
    p.y -= 94 * m_Game.m_Zoom + (1.1f - message.remaining) * 30;
    Color color = message.color;
    color.a = (std::min)(1.0f, message.remaining * 2);
    r.Text(p.x - r.TextWidth(message.text, 1.2f) * 0.5f, p.y, message.text, color, 1.2f);
}
