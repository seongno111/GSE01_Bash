#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"

#include <algorithm>
#include <cmath>

using namespace GameRules;

PlayerActor::PlayerActor(Prototype& game)
    : GameActor(game, "Player", TickGroup::Player, ActorLayer::World)
{
    SetLocalPosition(2, 1);
}

void PlayerActor::Update(float dt)
{
    attack = (std::max)(0.0f, attack - dt);
    hitCooldown = (std::max)(0.0f, hitCooldown - dt);

    const float right = float(m_Game.m_Keys['d']) - float(m_Game.m_Keys['a']);
    const float down = float(m_Game.m_Keys['s']) - float(m_Game.m_Keys['w']);
    const double dx = right + down;
    const double dy = down - right;
    const double length = Distance(dx, dy);
    const WorldPoint previous = WorldPosition();
    if (length > 0)
    {
        const double speed = PlayerWalkSpeed * (m_Game.m_RunHeld ? PlayerRunMultiplier : 1.0);
        double remaining = speed * dt;
        // Keep collision checks close together even while running at a low frame rate.
        while (remaining > 0)
        {
            const double step = (std::min)(remaining, 0.1);
            m_Game.MoveActor(*this, dx / length * step, dy / length * step, 0.18f);
            remaining -= step;
        }
    }
    walking = Distance(WorldX() - previous.x, WorldY() - previous.y) > 0.00001;
    running = walking && m_Game.m_RunHeld;
    magnetTime = (std::max)(0.0f, magnetTime - dt);
    shotCooldown = (std::max)(0.0f, shotCooldown - dt);
    m_Game.SelectTarget();
    if ((autoFire || m_Game.m_Keys[' ']) && shotCooldown <= 0)
    {
        m_Game.FireProjectile();
    }
}

void PlayerActor::Draw(Renderer& r)
{
    m_Game.DrawCharacter(r, m_Game.Project(WorldX(), WorldY()), corruption, false, SpritePose());
}
