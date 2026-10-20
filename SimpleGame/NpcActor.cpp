#include "stdafx.h"
#include "NpcWorld.h"
#include "Prototype.h"
#include "GameRules.h"
#include <algorithm>
#include <utility>
#include <cmath>

NpcActor::NpcActor(Prototype& game, NpcWorld& world, NpcProfile value, WorldPoint origin)
    : GameActor(game, value.name, TickGroup::Npc, ActorLayer::World),
      profile(std::move(value)),
      home(origin),
      m_World(world)
{
    SetLocalPosition(home.x, home.y);
    activityAnchor = home;
    birth = NpcBirthByRole[static_cast<size_t>(profile.role)];
    const float variation = 0.96f + float(GameRules::Hash(profile.id, 2, world.Seed()) % 9) * .01f;
    birth.health *= variation;
    birth.strength *= variation;
    vitals.health = vitals.maxHealth = birth.health;
    vitals.strength = birth.strength;
    vitals.holyPower = birth.holyPower;
    vitals.wisdom = birth.wisdom;
    vitals.armor = birth.armor;
    vitals.moveSpeed = birth.speed;
    vitals.gold = birth.gold;
    vitals.goods = profile.role == NpcRole::Merchant ? 3 : 0;
}

void NpcActor::Update(float dt)
{
    m_World.UpdateActor(*this, dt);
}

void NpcActor::Draw(Renderer& r)
{
    Point p = m_Game.Project(WorldX(), WorldY());
    const float z = m_Game.Zoom();
    const Color clothes[] = {Color(0.78f, 0.72f, 0.52f),
                             Color(0.36f, 0.49f, 0.63f),
                             Color(0.34f, 0.55f, 0.35f),
                             Color(0.58f, 0.25f, 0.22f),
                             Color(0.56f, 0.40f, 0.19f)};
    const Color color = clothes[static_cast<size_t>(profile.role)];
    if (!vitals.alive)
    {
        r.Ellipse(p, 16 * z, 6 * z, color.Shade(0.3f));
        return;
    }
    r.Ellipse({p.x, p.y + 2 * z}, 15 * z, 6 * z, Color(0.01f, 0.02f, 0.02f, 0.6f));
    if (m_World.Selected() == this)
    {
        r.Ellipse({p.x, p.y + 1 * z}, 21 * z, 9 * z, Color(0.65f, 0.75f, 0.32f, 0.28f));
    }
    if (execution.phase == NpcPhase::Perform
        && (execution.action == NpcAction::Attack || execution.action == NpcAction::Raid))
    {
        r.Ellipse(p, 35 * z, 18 * z, Color(0.9f, 0.15f, 0.1f, 0.25f));
    }
    const bool mutant = vitals.corruption >= GameRules::MutationThreshold;
    const unsigned sprite = mutant ? 9u : 1u + static_cast<unsigned>(profile.role);
    const Color tint = vitals.frenzy ? Color(1, 0.55f, 0.7f) : Color();
    if (!r.DrawUnitSprite(sprite, p, (mutant ? 78.0f : 72.0f) * z, SpritePose(), tint))
    {
        r.DrawCachedMesh(
            "npc/v1/" + std::to_string(int(profile.role)) + (mutant ? "/mutant" : "/human"),
            p,
            z,
            [&]()
            {
                r.Line({p.x - 5 * z, p.y}, {p.x - 6 * z, p.y - 18 * z}, 5 * z, GameRules::Ink);
                r.Line({p.x + 5 * z, p.y}, {p.x + 7 * z, p.y - 18 * z}, 5 * z, GameRules::Ink);
                r.Triangle({p.x - 14 * z, p.y - 7 * z},
                           {p.x + 14 * z, p.y - 7 * z},
                           {p.x, p.y - 38 * z},
                           color);
                r.Ellipse({p.x, p.y - 39 * z}, 8 * z, 10 * z, Color(0.62f, 0.56f, 0.45f));
                if (profile.role == NpcRole::Priest)
                {
                    r.Line({p.x + 16 * z, p.y},
                           {p.x + 16 * z, p.y - 52 * z},
                           3 * z,
                           GameRules::Gold);
                    r.Line({p.x + 10 * z, p.y - 45 * z},
                           {p.x + 22 * z, p.y - 45 * z},
                           3 * z,
                           GameRules::Gold);
                }
                else if (profile.role == NpcRole::Knight)
                {
                    r.Rect(p.x - 18 * z, p.y - 29 * z, 10 * z, 19 * z, Color(0.55f, 0.59f, 0.65f));
                    r.Line({p.x + 13 * z, p.y - 10 * z},
                           {p.x + 21 * z, p.y - 40 * z},
                           3 * z,
                           GameRules::Paper);
                }
                else if (profile.role == NpcRole::Merchant)
                {
                    r.Rect(p.x + 10 * z, p.y - 22 * z, 10 * z, 12 * z, GameRules::Gold.Shade(0.6f));
                }
                else
                {
                    r.Line({p.x + 13 * z, p.y - 8 * z},
                           {p.x + 20 * z, p.y - 32 * z},
                           3 * z,
                           GameRules::Paper);
                }
                if (mutant)
                {
                    r.Triangle({p.x - 8 * z, p.y - 43 * z},
                               {p.x - 18 * z, p.y - 60 * z},
                               {p.x - 3 * z, p.y - 47 * z},
                               GameRules::Violet);
                }
            },
            tint);
    }
}

void NpcActor::DrawOverlay(Renderer& r)
{
    const Point p = m_Game.Project(WorldX(), WorldY());
    const float z = m_Game.Zoom();
    const Color color = vitals.frenzy ? GameRules::Violet : GameRules::Paper;
    const float height = vitals.corruption >= GameRules::MutationThreshold ? 78.0f : 72.0f;
    const std::string title =
        profile.name
        + (!vitals.alive ? " · 사망"
                         : " · "
                               + std::string(mission.type != VillageMission::None
                                                 ? VillageMissionName(mission.type)
                                                 : NpcActivityName(profile.role, execution.action))
                               + " / " + NpcPhaseName(execution.phase));
    r.Text(p.x - r.TextWidth(title, 0.85f) * 0.5f, p.y - (height + 21) * z, title, color, 0.85f);
    if (vitals.alive)
    {
        r.Rect(p.x - 20 * z, p.y - (height + 6) * z, 40 * z, 3 * z, GameRules::Ink);
        r.Rect(p.x - 20 * z,
               p.y - (height + 6) * z,
               40 * z * vitals.health / vitals.maxHealth,
               3 * z,
               Color(0.35f, 0.65f, 0.42f));
    }
}

NpcMissileActor::NpcMissileActor(Prototype& game,
                                 NpcWorld& world,
                                 NpcEntity owner,
                                 NpcEntity target,
                                 NpcRole role,
                                 WorldPoint origin,
                                 WorldPoint aim,
                                 float damage)
    : GameActor(game, "NpcMissile", TickGroup::Projectile, ActorLayer::Effects),
      m_World(world),
      m_Owner(owner),
      m_Target(target),
      m_Role(role),
      m_Damage(damage),
      m_Remaining(NpcWeaponFor(role).range)
{
    SetWorldPosition(origin.x, origin.y);
    const double length = GameRules::Length(aim.x - origin.x, aim.y - origin.y);
    m_Direction = length > 0.001
                      ? WorldPoint{(aim.x - origin.x) / length, (aim.y - origin.y) / length}
                      : WorldPoint{1, 0};
}

void NpcMissileActor::Update(float dt)
{
    auto* target = m_World.Resolve(m_Target);
    if (!target || !m_World.Alive(m_Target))
    {
        Destroy();
        return;
    }
    float movement = (std::min)(m_Remaining, NpcWeaponFor(m_Role).speed * dt);
    while (movement > 0)
    {
        const float step = (std::min)(movement, 0.08f);
        TranslateWorld(m_Direction.x * step, m_Direction.y * step);
        m_Remaining -= step;
        movement -= step;
        if (m_Game.Blocked(WorldX(), WorldY()) || GameRules::InsideSanctuary(WorldX(), WorldY()))
        {
            Destroy();
            return;
        }
        // Fixed launch direction; only the intended target is eligible (no friendly fire).
        const auto* enemy = dynamic_cast<const EnemyActor*>(target);
        const double radius = enemy && enemy->tier == 2 ? 0.58 : 0.38;
        if (GameRules::Length(WorldX() - target->WorldX(), WorldY() - target->WorldY()) < radius)
        {
            m_World.Damage(m_Owner, m_Target, m_Damage);
            m_World.Event(std::string(NpcWeaponFor(m_Role).name) + " 명중 · 공격자 "
                          + std::to_string(m_Owner) + " 대상 " + std::to_string(m_Target));
            Destroy();
            return;
        }
    }
    if (m_Remaining <= 0)
    {
        Destroy();
    }
}

void NpcMissileActor::Draw(Renderer& r)
{
    Point head = m_Game.Project(WorldX(), WorldY());
    Point tail = m_Game.Project(WorldX() - m_Direction.x * 0.35, WorldY() - m_Direction.y * 0.35);
    const float z = m_Game.Zoom();
    head.y -= 36 * z;
    tail.y -= 36 * z;
    const Color color = m_Role == NpcRole::Priest       ? GameRules::Gold.Radiance(3.0f)
                        : m_Role == NpcRole::Adventurer ? Color(0.65f, 0.9f, 0.65f)
                        : m_Role == NpcRole::Raider     ? Color(0.9f, 0.4f, 0.35f)
                                                        : Color(0.7f, 0.7f, 0.75f);
    // Renderer caches these primitive meshes and batches their instances.
    r.Line(tail, head, (m_Role == NpcRole::Raider ? 4 : 2) * z, color);
    if (m_Role == NpcRole::Priest || m_Role == NpcRole::Merchant)
    {
        r.Ellipse(head, 4 * z, 4 * z, color);
    }
}
