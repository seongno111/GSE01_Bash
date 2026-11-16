#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"
#include "HeroWeapons.h"

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

    shotCooldown = (std::max)(0.0f, shotCooldown - dt);
    skillCooldown = (std::max)(0.0f, skillCooldown - dt);
    magnetTime = (std::max)(0.0f, magnetTime - dt);
    if (m_Game.m_WeaponShopOpen)
    {
        if (weaponAttackStart >= 0)
        {
            ResetAnimation();
        }
        walking = false;
        running = false;
        return;
    }
    if (m_Game.UpdateWeaponSkill())
    {
        walking = false;
        running = false;
        return;
    }
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
    if (weaponAttackStart >= 0 && attackMutant != (corruption >= MutationThreshold))
    {
        ResetAnimation();
    }
    if (attackReleaseAt >= 0)
    {
        double x = 0;
        double y = 0;
        if (m_Game.TargetPosition(x, y, attackTarget))
        {
            AimAttackAnimation(x - WorldX(), y - WorldY());
        }
    }
    if (weapon == 1 && weaponAttack && attackReleaseAt >= 0)
    {
        const auto* visual = m_Game.ModelAssets().HeroWeaponActions(HeroWeapons[weapon].key);
        const float rate = weaponAttack->duration / weaponAttackDuration;
        if (visual && (m_Game.Time() - weaponAttackStart) * rate >= visual->cancelAfter)
        {
            double x = 0;
            double y = 0;
            if (!m_Game.TargetPosition(x, y, attackTarget)
                || Distance(x - WorldX(), y - WorldY()) > range
                || !m_Game.ClearShot(WorldX(), WorldY(), x, y))
            {
                // Lower from the drawn pose when the locked target can no longer be hit.
                weaponAttack = &visual->cancel;
                weaponAttackStart = m_Game.Time();
                weaponAttackDuration = visual->cancel.duration / rate;
                attackReleaseAt = -1;
                shotCooldown = (std::max)(shotCooldown, weaponAttackDuration);
                BeginAttackAnimation(std::sin(ModelYaw()),
                                     std::cos(ModelYaw()),
                                     weaponAttackDuration,
                                     true);
            }
        }
    }
    if (attackReleaseAt >= 0 && m_Game.Time() >= attackReleaseAt)
    {
        m_Game.ResolvePlayerAttack();
    }
    m_Game.SelectTarget();
    if ((autoFire || m_Game.m_Keys[' ']) && shotCooldown <= 0)
    {
        m_Game.FireProjectile();
    }
}

void PlayerActor::Draw(Renderer& r)
{
    const auto pose = SpritePose();
    const bool mutant = corruption >= MutationThreshold;
    std::string clip =
        mutant ? (pose.attackProgress >= 0 ? "MON_03_Attack" : MovementClip("MON_03", running))
        : pose.attackProgress >= 0 ? "HERO_AttackLongsword"
                                   : MovementClip("HERO", running);
    float progress = pose.attackProgress;
    const char* skillClip = nullptr;
    float spinYaw = 0;
    if (!mutant)
    {
        if (const auto* visual = m_Game.ModelAssets().HeroWeaponActions(HeroWeapons[weapon].key))
        {
            clip = visual->prefix + (pose.moving ? (running ? "_Run" : "_Walk") : "_Ready");
            if (progress >= 0 && weaponAttack)
            {
                float time = progress * weaponAttack->duration;
                for (const auto& stage : weaponAttack->stages)
                {
                    clip = stage.clip;
                    // Aim is a short hold from a longer loop, not an accelerated full loop.
                    progress = std::clamp(time / stage.clipDuration, 0.0f, 1.0f);
                    if (time < stage.duration)
                    {
                        break;
                    }
                    time -= stage.duration;
                }
            }
            else
            {
                progress = -1;
            }
        }
    }
    if (!mutant && skillCast.startedAt >= 0)
    {
        const auto& skill = HeroWeapons[weapon].skill;
        const float elapsed = m_Game.Time() - skillCast.startedAt;
        skillClip = skillCast.clip;
        progress = std::clamp(elapsed / skillCast.duration, 0.0f, 1.0f);
        if (const auto* visual = m_Game.ModelAssets().HeroWeaponActions(HeroWeapons[weapon].key))
        {
            clip = visual->attacks.front().stages.back().clip;
        }
        if (!skillClip && skill.kind == HeroSkillKind::Whirlwind)
        {
            spinYaw = 12.5663706f * std::clamp((elapsed - .15f) / .8f, 0.0f, 1.0f);
        }
    }
    if (DrawModel(
            r,
            mutant ? "player/mutated" : std::string("player/") + HeroWeapons[weapon].key,
            [&]()
            {
                if (mutant)
                {
                    return m_Game.ModelAssets().Monster(3);
                }
                const auto equipped =
                    m_Game.ModelAssets().HeroWeaponModel(HeroWeapons[weapon].key, true);
                return equipped ? equipped : m_Game.ModelAssets().Player();
            },
            clip,
            progress,
            Color(),
            progress >= 0 ? 0.0f : .12f,
            skillClip,
            spinYaw))
    {
        return;
    }
    m_Game.DrawCharacter(r, m_Game.Project(WorldX(), WorldY()), corruption, false, SpritePose());
}

void PlayerActor::ResetAnimation()
{
    GameActor::ResetAnimation();
    skillCast = {};
    weaponAttack = nullptr;
    weaponAttackStart = -1;
    weaponAttackDuration = 0;
    attackReleaseAt = -1;
    attackTarget = InvalidActor;
    attackWeapon = -1;
    nextAttack = 0;
}
