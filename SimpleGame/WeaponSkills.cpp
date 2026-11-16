#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"
#include "HeroWeapons.h"
#include "NpcWorld.h"

#include <algorithm>
#include <cmath>

using namespace GameRules;

namespace
{
    constexpr double ToRadians = 0.017453292519943295;
    constexpr int FanArrows = 5;
    constexpr float HammerStun = 2;
    constexpr float BossStun = .8f;

    WorldPoint Offset(WorldPoint origin, double yaw, double distance, double side = 0)
    {
        return {origin.x + std::sin(yaw) * distance + std::cos(yaw) * side,
                origin.y + std::cos(yaw) * distance - std::sin(yaw) * side};
    }

    double FanAngle(double yaw, float halfAngle, int arrow)
    {
        return yaw + halfAngle * ToRadians * (2.0 * arrow / (FanArrows - 1) - 1);
    }
} // namespace

std::vector<ActorId> Prototype::WeaponSkillTargets() const
{
    std::vector<ActorId> targets;
    for (const auto& enemy : m_Enemies)
    {
        if (enemy.alive && enemy.EnabledInHierarchy())
        {
            targets.push_back(enemy.Id());
        }
    }
    for (const auto& echo : m_Echoes)
    {
        if (echo.alive && echo.EnabledInHierarchy())
        {
            targets.push_back(echo.Id());
        }
    }
    if (m_NpcWorld)
    {
        for (auto id : m_NpcWorld->Actors())
        {
            const auto* npc = dynamic_cast<const NpcActor*>(m_Scene.Find(id));
            if (npc && npc->EnabledInHierarchy() && m_NpcWorld->PlayerCanTarget(*npc))
            {
                targets.push_back(id);
            }
        }
    }
    return targets;
}

void Prototype::BeginWeaponSkill()
{
    if (!m_LevelOneStarted || m_Player.skillCast.startedAt >= 0 || m_Player.health <= 0
        || !m_Player.EnabledInHierarchy())
    {
        return;
    }
    if (m_Player.corruption >= MutationThreshold
        || InsideSanctuary(m_Player.WorldX(), m_Player.WorldY()))
    {
        Notice("무기 스킬은 성소 밖에서 인간 형태로 사용할 수 있습니다.");
        return;
    }
    if (m_Player.skillCooldown > 0)
    {
        Notice("무기 스킬을 다시 사용할 준비가 되지 않았습니다.");
        return;
    }

    const auto& weapon = HeroWeapons[m_Player.weapon];
    const auto& skill = weapon.skill;
    const auto origin = m_Player.WorldPosition();
    PlayerActor::SkillCast cast;
    cast.startedAt = m_Time;
    cast.duration = skill.duration;
    cast.yaw = m_Player.ModelYaw();
    cast.center = origin;
    cast.damage = ProjectileDamage() * skill.damageScale;
    cast.contacts.reserve(skill.hits);
    if (const auto* clip = m_ModelAssets.HeroWeaponSkill(weapon))
    {
        cast.clip = skill.clip;
        cast.duration = clip->duration;
        for (const auto& event : clip->events)
        {
            if (event.name == "skill_contact")
            {
                cast.contacts.push_back(event.time);
            }
        }
    }
    else
    {
        for (int hit = 0; hit < skill.hits; ++hit)
        {
            cast.contacts.push_back(skill.firstHit + hit * skill.hitInterval);
        }
    }
    if (skill.kind != HeroSkillKind::Whirlwind)
    {
        double nearest = skill.range + .0001;
        for (auto id : WeaponSkillTargets())
        {
            const auto point = m_Scene.Find(id)->WorldPosition();
            const double distance = Length(point.x - origin.x, point.y - origin.y);
            if (distance < nearest && !InsideSanctuary(point.x, point.y)
                && ClearShot(origin.x, origin.y, point.x, point.y))
            {
                nearest = distance;
                cast.target = id;
                cast.center = point;
            }
        }
        if (cast.target != InvalidActor)
        {
            const double dx = cast.center.x - origin.x;
            const double dy = cast.center.y - origin.y;
            if (Length(dx, dy) > .0001)
            {
                cast.yaw = std::atan2(dx, dy);
            }
        }
        else if (skill.kind == HeroSkillKind::Flurry)
        {
            Notice("단검 스킬의 사거리 안에 대상이 없습니다.");
            return;
        }
        else if (skill.kind == HeroSkillKind::AshField)
        {
            // Place the field along the facing direction, stopping at the first obstacle.
            for (double distance = .1; distance <= skill.range; distance += .1)
            {
                const auto point = Offset(origin, cast.yaw, distance);
                if (Blocked(point.x, point.y))
                {
                    break;
                }
                cast.center = point;
            }
        }
    }

    m_Player.weaponAttack = nullptr;
    m_Player.weaponAttackStart = -1;
    m_Player.attackReleaseAt = -1;
    m_Player.skillCooldown = skill.cooldown;
    m_Player.BeginAttackAnimation(std::sin(cast.yaw), std::cos(cast.yaw), cast.duration, true);
    m_Player.skillCast = std::move(cast);
}

bool Prototype::UpdateWeaponSkill()
{
    auto& cast = m_Player.skillCast;
    if (cast.startedAt < 0)
    {
        return false;
    }
    if (m_Player.health <= 0 || m_Player.corruption >= MutationThreshold
        || InsideSanctuary(m_Player.WorldX(), m_Player.WorldY()))
    {
        m_Player.ResetAnimation();
        return false;
    }

    const float elapsed = m_Time - cast.startedAt;
    while (cast.nextHit < cast.contacts.size() && elapsed >= cast.contacts[cast.nextHit])
    {
        ++cast.nextHit;
        ResolveWeaponSkill();
    }
    if (elapsed >= cast.duration)
    {
        cast = {};
        return false;
    }
    return true;
}

void Prototype::ApplyPlayerHit(::Actor& target, float damage, float stun)
{
    if (auto* enemy = dynamic_cast<EnemyActor*>(&target))
    {
        const float applied = enemy->ReceiveDamage(damage, m_Player.Id());
        AddCombatText(enemy->WorldX(), enemy->WorldY(), std::to_string(int(applied)), DamageColor);
        if (enemy->health <= 0)
        {
            DefeatEnemy(*enemy);
        }
        else if (stun > 0)
        {
            enemy->attackWindup = 0;
            enemy->burstWindup = 0;
            enemy->specialWindup = 0;
            enemy->chargeRemaining = 0;
            enemy->specialRecovery = false;
            enemy->state = MonsterState::Recovering;
            enemy->ResetAnimation();
            enemy->StunFor(enemy->Definition().boss ? BossStun : stun);
            AddCombatText(enemy->WorldX(), enemy->WorldY() + .3, "기절", Gold);
        }
    }
    else if (auto* echo = dynamic_cast<EchoActor*>(&target))
    {
        echo->health -= damage;
        AddCombatText(echo->WorldX(), echo->WorldY(), std::to_string(int(damage)), DamageColor);
        if (echo->health <= 0)
        {
            echo->alive = false;
            if (m_NpcWorld)
            {
                m_NpcWorld->RecordWorldEvent("hunt",
                                             "플레이어가 변이 잔재를 토벌했다",
                                             echo->WorldPosition(),
                                             &m_Player,
                                             echo);
            }
        }
        else if (stun > 0)
        {
            echo->StunFor(stun);
            AddCombatText(echo->WorldX(), echo->WorldY() + .3, "기절", Gold);
        }
    }
    else if (auto* npc = dynamic_cast<NpcActor*>(&target); npc && m_NpcWorld)
    {
        m_NpcWorld->HitByPlayer(*npc, damage, stun);
    }
}

void Prototype::ResolveWeaponSkill()
{
    const auto& skill = HeroWeapons[m_Player.weapon].skill;
    const auto& cast = m_Player.skillCast;
    const auto origin = m_Player.WorldPosition();
    if (skill.kind == HeroSkillKind::ArrowFan)
    {
        for (int i = 0; i < FanArrows; ++i)
        {
            const double yaw = FanAngle(cast.yaw, skill.width, i);
            auto& shot = m_Projectiles.Spawn(*this,
                                             origin.x,
                                             origin.y,
                                             std::sin(yaw),
                                             std::cos(yaw),
                                             cast.damage,
                                             skill.range);
            shot.arrow = true;
        }
        return;
    }

    const bool ground = skill.kind == HeroSkillKind::AshField;
    const auto center = ground ? cast.center : origin;
    const double forwardX = std::sin(cast.yaw);
    const double forwardY = std::cos(cast.yaw);
    const double coneLimit = std::cos(skill.width * ToRadians);
    const auto targets = skill.kind == HeroSkillKind::Flurry ? std::vector<ActorId>{cast.target}
                                                             : WeaponSkillTargets();
    for (auto id : targets)
    {
        double x = 0;
        double y = 0;
        if (id == InvalidActor || !TargetPosition(x, y, id) || InsideSanctuary(x, y))
        {
            continue;
        }
        const double dx = x - center.x;
        const double dy = y - center.y;
        const double distance = Length(dx, dy);
        const double forward = dx * forwardX + dy * forwardY;
        bool hit = distance <= (ground ? skill.width : skill.range);
        if (skill.kind == HeroSkillKind::LongThrust)
        {
            hit = forward >= 0 && forward <= skill.range
                  && std::abs(dx * forwardY - dy * forwardX) <= skill.width;
        }
        else if (skill.kind == HeroSkillKind::GuardBreak || skill.kind == HeroSkillKind::Execution
                 || skill.kind == HeroSkillKind::GroundSlam)
        {
            hit = hit && forward >= distance * coneLimit;
        }
        if (!hit || !ClearShot(center.x, center.y, x, y))
        {
            continue;
        }

        auto& actor = *m_Scene.Find(id);
        auto* enemy = dynamic_cast<EnemyActor*>(&actor);
        if (skill.kind == HeroSkillKind::GuardBreak && enemy && enemy->guardRemaining > 0)
        {
            enemy->guardRemaining = 0;
            enemy->wardEndedAt = m_Time;
        }
        float damage = cast.damage;
        if (skill.kind == HeroSkillKind::Execution)
        {
            const auto* echo = dynamic_cast<const EchoActor*>(&actor);
            const auto* npc = dynamic_cast<const NpcActor*>(&actor);
            const bool wounded = enemy  ? enemy->health <= enemy->maxHealth * .4f
                                 : echo ? echo->health <= echo->maxHealth * .4f
                                        : npc && npc->vitals.health <= npc->vitals.maxHealth * .4f;
            if (wounded)
            {
                damage *= 1.6f;
            }
        }
        ApplyPlayerHit(actor, damage, skill.kind == HeroSkillKind::GroundSlam ? HammerStun : 0);
        if (skill.kind == HeroSkillKind::GuardBreak && (!enemy || !enemy->Definition().boss)
            && TargetPosition(x, y, id))
        {
            CancelRoute(id);
            if (enemy)
            {
                enemy->route.clear();
                enemy->nextRoute = 0;
            }
            else if (auto* npc = dynamic_cast<NpcActor*>(&actor))
            {
                npc->path.clear();
                npc->nextPath = 0;
            }
            for (int step = 0; step < 10; ++step)
            {
                MoveActor(actor, forwardX * .1, forwardY * .1, .18f);
            }
        }
    }
}

void Prototype::DrawWeaponSkill(Renderer& renderer)
{
    const auto& cast = m_Player.skillCast;
    if (cast.startedAt < 0)
    {
        return;
    }
    const auto& skill = HeroWeapons[m_Player.weapon].skill;
    const auto origin = m_Player.WorldPosition();
    const Color color =
        cast.nextHit > 0 ? Color(1, .56f, .19f, .88f) : Color(.94f, .81f, .45f, .65f);
    const auto line = [&](WorldPoint a, WorldPoint b)
    {
        renderer.Line(Project(a.x, a.y), Project(b.x, b.y), 2 * m_Zoom, color);
    };
    if (skill.kind == HeroSkillKind::ArrowFan)
    {
        for (int i = 0; i < FanArrows; ++i)
        {
            line(origin, Offset(origin, FanAngle(cast.yaw, skill.width, i), skill.range));
        }
        return;
    }
    if (skill.kind == HeroSkillKind::LongThrust)
    {
        const auto a = Offset(origin, cast.yaw, 0, -skill.width);
        const auto b = Offset(origin, cast.yaw, skill.range, -skill.width);
        const auto c = Offset(origin, cast.yaw, skill.range, skill.width);
        const auto d = Offset(origin, cast.yaw, 0, skill.width);
        line(a, b);
        line(b, c);
        line(c, d);
        line(d, a);
        return;
    }
    const bool cone = skill.kind == HeroSkillKind::GuardBreak
                      || skill.kind == HeroSkillKind::Execution
                      || skill.kind == HeroSkillKind::GroundSlam;
    const bool targeted =
        skill.kind == HeroSkillKind::AshField || skill.kind == HeroSkillKind::Flurry;
    const auto center = targeted ? cast.center : origin;
    const double radius = skill.kind == HeroSkillKind::AshField ? skill.width
                          : skill.kind == HeroSkillKind::Flurry ? .5
                                                                : skill.range;
    const double halfAngle = (cone ? skill.width : 180.0) * ToRadians;
    auto edge = Offset(center, cast.yaw - halfAngle, radius);
    if (cone)
    {
        line(center, edge);
    }
    constexpr int Segments = 24;
    for (int i = 1; i <= Segments; ++i)
    {
        const auto next = Offset(center, cast.yaw + halfAngle * (2.0 * i / Segments - 1), radius);
        line(edge, next);
        edge = next;
    }
    if (cone)
    {
        line(edge, center);
    }
}
