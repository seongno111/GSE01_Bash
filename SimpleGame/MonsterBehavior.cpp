#include "stdafx.h"
#include "Prototype.h"
#include "NpcWorld.h"
#include "GameRules.h"

#include <algorithm>
#include <cmath>

using namespace GameRules;

void EnemyActor::ResetCombat()
{
    attackWindup = 0;
    burstWindup = 0;
    recovery = 0;
    specialWindup = 0;
    specialCooldown = Definition().specialCooldown * .5f;
    guardRemaining = 0;
    chargeRemaining = 0;
    chargeHit = false;
    enraged = false;
    specialRecovery = false;
    wardEndedAt = -100;
    visualTimerSampleAt = m_Game.Time();
    packNearby = 1;
    combatTarget = InvalidActor;
    supportTarget = InvalidActor;
    alertTarget = InvalidActor;
    alertUntil = 0;
    nextTargetScan = 0;
    nextPackScan = 0;
    lastHurtTime = -100;
    route.clear();
    routeIndex = 0;
    nextRoute = 0;
    state = MonsterState::Patrol;
}

float EnemyActor::ReceiveDamage(float amount, ActorId attacker)
{
    if (!alive || !std::isfinite(amount) || amount <= 0)
    {
        return 0;
    }
    const float applied =
        amount * 100 / (100 + Definition().armor) * (guardRemaining > 0 ? .35f : 1.0f);
    health = (std::max)(0.0f, health - applied);
    lastHurtTime = m_Game.Time();
    if (attacker != InvalidActor)
    {
        alertTarget = attacker;
        alertUntil = m_Game.Time() + 5;
        nextTargetScan = 0;
    }
    return applied;
}

void EnemyActor::DamageInArea(WorldPoint center, float radius, float amount)
{
    const auto hit = [&](Actor& actor)
    {
        if (!actor.EnabledInHierarchy()
            || Length(actor.WorldX() - center.x, actor.WorldY() - center.y) > radius
            || !m_Game.ClearShot(center.x, center.y, actor.WorldX(), actor.WorldY()))
        {
            return;
        }
        if (m_Game.m_NpcWorld)
        {
            m_Game.m_NpcWorld->MonsterDamage(actor, amount, this);
        }
    };
    hit(m_Game.Player());
    if (m_Game.m_NpcWorld)
    {
        for (const auto id : m_Game.m_NpcWorld->Actors())
        {
            auto* npc = dynamic_cast<NpcActor*>(m_Game.Scene().Find(id));
            if (npc && npc->vitals.alive)
            {
                hit(*npc);
            }
        }
    }
}

void EnemyActor::ShootMonsterProjectile(const Actor& target, int count, float spread, float amount)
{
    const double angle = std::atan2(target.WorldY() - WorldY(), target.WorldX() - WorldX());
    for (int i = 0; i < count; ++i)
    {
        const double direction = angle + (i - (count - 1) * .5) * spread;
        auto& shot = m_Game.m_EnemyProjectiles.Spawn(m_Game,
                                                     WorldX(),
                                                     WorldY(),
                                                     std::cos(direction),
                                                     std::sin(direction),
                                                     amount,
                                                     Definition().attackRange + 2,
                                                     true);
        shot.source = Id();
        shot.travelSpeed = kind == MonsterKind::HighDemon ? 5.4f : 4.6f;
        shot.tint =
            kind == MonsterKind::HighDemon ? Color(.92f, .16f, .09f) : Color(.6f, .24f, .78f);
    }
}

void EnemyActor::ResolveMonsterSpecial()
{
    const auto& stats = Definition();
    specialRecovery = true;
    const float power = enraged ? 1.25f : 1;
    if (stats.ability == MonsterAbility::Pounce || stats.ability == MonsterAbility::Charge)
    {
        const double dx = specialAim.x - WorldX();
        const double dy = specialAim.y - WorldY();
        const double distance = Length(dx, dy);
        if (distance > .01)
        {
            chargeDirection = {dx / distance, dy / distance};
            chargeRemaining = stats.ability == MonsterAbility::Pounce ? 3.0f : 6.0f;
            chargeHit = false;
            state = MonsterState::Charging;
        }
    }
    else if (stats.ability == MonsterAbility::Slam || stats.ability == MonsterAbility::Rift)
    {
        DamageInArea(specialAim, stats.specialRadius, stats.specialDamage * power);
        recovery = stats.attackRecovery;
    }
    else if (stats.ability == MonsterAbility::Ward)
    {
        guardRemaining = 2.4f;
        recovery = .25f;
    }
    else if (stats.ability == MonsterAbility::Mend)
    {
        auto* ally = dynamic_cast<EnemyActor*>(m_Game.Scene().Find(supportTarget));
        if (ally && ally->alive && ally->EnabledInHierarchy()
            && Length(ally->WorldX() - WorldX(), ally->WorldY() - WorldY()) <= stats.specialRange
            && m_Game.ClearShot(WorldX(), WorldY(), ally->WorldX(), ally->WorldY()))
        {
            const float restored = (std::min)(stats.specialDamage, ally->maxHealth - ally->health);
            ally->health += restored;
            if (restored > 0 && m_Game.m_NpcWorld)
            {
                m_Game.AddCombatText(ally->WorldX(),
                                     ally->WorldY(),
                                     "+" + std::to_string(int(restored)),
                                     Color(.67f, .35f, .83f));
                m_Game.m_NpcWorld->RecordWorldEvent(
                    "monster_heal",
                    std::string(stats.name) + "의 오염 봉합: " + ally->Definition().name,
                    ally->WorldPosition(),
                    this,
                    ally);
            }
        }
        supportTarget = InvalidActor;
        recovery = .5f;
    }
    specialCooldown = stats.specialCooldown * (enraged ? .75f : 1.0f);
}

bool EnemyActor::UpdateMonsterTactics(float dt, Actor* target, bool forcedReturn)
{
    if (!UsesMonsterTactics())
    {
        return false;
    }
    const auto& stats = Definition();
    const float now = m_Game.Time();
    visualTimerSampleAt = now;
    specialCooldown = (std::max)(0.0f, specialCooldown - dt);
    const bool wasGuarding = guardRemaining > 0;
    guardRemaining = (std::max)(0.0f, guardRemaining - dt);
    if (wasGuarding && guardRemaining <= 0)
    {
        wardEndedAt = now;
    }
    recovery = (std::max)(0.0f, recovery - dt);
    const bool shouldEnrage = (kind == MonsterKind::BlightBear && health <= maxHealth * .4f)
                              || (kind == MonsterKind::HighDemon && health <= maxHealth * .5f);
    if (shouldEnrage && !enraged && m_Game.m_NpcWorld)
    {
        m_Game.m_NpcWorld->RecordWorldEvent("monster_enrage",
                                            std::string(stats.name) + " 광폭화",
                                            WorldPosition(),
                                            this);
    }
    enraged = shouldEnrage;
    if (forcedReturn)
    {
        attackWindup = 0;
        specialWindup = 0;
        chargeRemaining = 0;
        supportTarget = InvalidActor;
        specialRecovery = false;
        state = MonsterState::Returning;
        return false;
    }
    const double distance =
        target ? Length(target->WorldX() - WorldX(), target->WorldY() - WorldY()) : 1e9;
    const float power = enraged ? 1.25f : 1;

    if (chargeRemaining > 0)
    {
        state = MonsterState::Charging;
        float movement = (std::min)(chargeRemaining, speed * 3.2f * dt);
        // Substeps prevent a charge from tunnelling through walls or a target at a low FPS.
        while (movement > 0 && chargeRemaining > 0)
        {
            const float step = (std::min)(movement, .12f);
            const auto before = WorldPosition();
            m_Game.MoveActor(*this, chargeDirection.x * step, chargeDirection.y * step, .18f);
            movement -= step;
            chargeRemaining -= step;
            if (Length(WorldX() - before.x, WorldY() - before.y) < step * .9)
            {
                chargeRemaining = 0;
            }
            if (!chargeHit && target
                && Length(target->WorldX() - WorldX(), target->WorldY() - WorldY())
                       < stats.specialRadius
                && m_Game.ClearShot(WorldX(), WorldY(), target->WorldX(), target->WorldY()))
            {
                if (m_Game.m_NpcWorld)
                {
                    m_Game.m_NpcWorld->MonsterDamage(*target, stats.specialDamage, this);
                }
                chargeHit = true;
                chargeRemaining = 0;
            }
        }
        if (chargeRemaining <= 0)
        {
            recovery = stats.attackRecovery;
        }
        return true;
    }
    if (specialWindup > 0)
    {
        state =
            stats.ability == MonsterAbility::Mend ? MonsterState::Supporting : MonsterState::Windup;
        specialWindup = (std::max)(0.0f, specialWindup - dt);
        if (specialWindup <= 0)
        {
            ResolveMonsterSpecial();
        }
        return true;
    }
    if (attackWindup > 0)
    {
        state = MonsterState::Windup;
        attackWindup = (std::max)(0.0f, attackWindup - dt);
        if (attackWindup <= 0)
        {
            recovery = stats.attackRecovery;
            if (target && distance <= stats.attackRange + .3
                && m_Game.ClearShot(WorldX(), WorldY(), target->WorldX(), target->WorldY()))
            {
                if (kind == MonsterKind::HighDemon || kind == MonsterKind::FallenPriest)
                {
                    ShootMonsterProjectile(*target,
                                           kind == MonsterKind::HighDemon ? (enraged ? 5 : 3) : 1,
                                           .18f,
                                           damage * power);
                }
                else if (m_Game.m_NpcWorld)
                {
                    const float packPower =
                        kind == MonsterKind::AshWolf ? 1 + .08f * (std::min)(3, packNearby - 1) : 1;
                    m_Game.m_NpcWorld->MonsterDamage(*target, damage * power * packPower, this);
                }
            }
        }
        return true;
    }
    if (recovery > 0)
    {
        state = MonsterState::Recovering;
        return true;
    }

    // Priests search only when their cast is available, not every rendered frame.
    if (stats.ability == MonsterAbility::Mend && specialCooldown <= 0)
    {
        EnemyActor* chosen = nullptr;
        float lowest = .70f;
        for (auto& ally : m_Game.m_Enemies)
        {
            if (&ally != this && ally.alive && ally.EnabledInHierarchy()
                && ally.health / ally.maxHealth < lowest
                && Length(ally.WorldX() - WorldX(), ally.WorldY() - WorldY()) <= stats.specialRange
                && m_Game.ClearShot(WorldX(), WorldY(), ally.WorldX(), ally.WorldY()))
            {
                chosen = &ally;
                lowest = ally.health / ally.maxHealth;
            }
        }
        if (!chosen && health < maxHealth * .35f)
        {
            chosen = this;
        }
        if (chosen)
        {
            supportTarget = chosen->Id();
            specialAim = chosen->WorldPosition();
            specialWindup = stats.specialWindup;
            specialRecovery = true;
            BeginAttackAnimation(specialAim.x - WorldX(),
                                 specialAim.y - WorldY(),
                                 stats.specialWindup);
            state = MonsterState::Supporting;
            return true;
        }
        specialCooldown = .6f;
    }
    if (!target)
    {
        state = MonsterState::Patrol;
        return false;
    }

    if (kind == MonsterKind::AshWolf && now >= nextPackScan)
    {
        nextPackScan = now + .75;
        packNearby = 1;
        for (auto& ally : m_Game.m_Enemies)
        {
            if (&ally != this && ally.kind == kind && ally.alive && ally.EnabledInHierarchy()
                && ally.outpost == outpost
                && Length(ally.WorldX() - WorldX(), ally.WorldY() - WorldY()) < 8)
            {
                ++packNearby;
                ally.alertTarget = target->Id();
                ally.alertUntil = now + 5;
            }
        }
    }
    const auto beginSpecial = [&]()
    {
        specialWindup = stats.specialWindup;
        specialAim =
            stats.ability == MonsterAbility::Slam ? WorldPosition() : target->WorldPosition();
        specialRecovery = true;
        BeginAttackAnimation(target->WorldX() - WorldX(),
                             target->WorldY() - WorldY(),
                             stats.specialWindup);
        state = MonsterState::Windup;
        if (m_Game.m_NpcWorld)
        {
            m_Game.m_NpcWorld->RecordWorldEvent("monster_ability",
                                                std::string(stats.name) + ": "
                                                    + MonsterAbilityName(stats.ability) + " 예고",
                                                specialAim,
                                                this,
                                                target);
        }
    };
    const bool stagThreatened = kind != MonsterKind::ThornStag || distance < 3.2
                                || now - lastHurtTime < 6 || health < maxHealth * .65f
                                || raidVillage >= 0;
    if (specialCooldown <= 0 && stats.ability != MonsterAbility::Mend
        && distance <= stats.specialRange && stagThreatened
        && m_Game.ClearShot(WorldX(), WorldY(), target->WorldX(), target->WorldY()))
    {
        beginSpecial();
        return true;
    }
    if (distance <= stats.attackRange && (kind != MonsterKind::FallenPriest || distance >= 3))
    {
        attackWindup = stats.attackWindup;
        specialRecovery = false;
        state = MonsterState::Windup;
        BeginAttackAnimation(target->WorldX() - WorldX(),
                             target->WorldY() - WorldY(),
                             stats.attackWindup + stats.attackRecovery);
        return true;
    }

    WorldPoint destination = target->WorldPosition();
    float travelSpeed = speed * (enraged ? 1.3f : 1);
    state = guardRemaining > 0 ? MonsterState::Guarding : MonsterState::Chase;
    if (guardRemaining > 0)
    {
        travelSpeed *= .6f;
    }
    const bool retreat = (kind == MonsterKind::ThornStag && !stagThreatened && distance < 5)
                         || (kind == MonsterKind::FallenPriest && distance < 3);
    if (retreat)
    {
        const double inverse = 1 / (std::max)(.01, distance);
        destination = {WorldX() + (WorldX() - target->WorldX()) * inverse * 2,
                       WorldY() + (WorldY() - target->WorldY()) * inverse * 2};
        state = MonsterState::Fleeing;
    }
    else if (kind == MonsterKind::AshWolf && packNearby > 1 && distance > 1.8 && distance < 5
             && std::fmod(now + float(Id() % 4) * .4f, 3.2f) < 1.6f)
    {
        const double angle = std::atan2(WorldY() - target->WorldY(), WorldX() - target->WorldX())
                             + (Id() % 2 ? .8 : -.8);
        destination = {target->WorldX() + std::cos(angle) * 1.4,
                       target->WorldY() + std::sin(angle) * 1.4};
        state = MonsterState::Flank;
    }
    // Respect the tactical tether even when kiting. Outpost raids own a larger strategic tether.
    const double leash = outpost >= 0 ? patrolRadius + 2 : stats.leashRange;
    if (raidVillage < 0 && Length(destination.x - homeX, destination.y - homeY) > leash)
    {
        destination = {homeX, homeY};
        state = MonsterState::Returning;
    }
    m_Game.MoveEnemy(*this, destination.x, destination.y, travelSpeed, dt);
    return true;
}
