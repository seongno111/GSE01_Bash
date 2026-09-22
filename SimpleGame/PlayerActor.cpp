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
    walking = length > 0;
    if (walking)
    {
        m_Game.MoveActor(*this, dx / length * 3.7 * dt, dy / length * 3.7 * dt, 0.18f);
    }
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
    m_Game.DrawCharacter(r, m_Game.Project(WorldX(), WorldY()), corruption, false);
}
