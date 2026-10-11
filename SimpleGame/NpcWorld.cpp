#include "stdafx.h"
#include "NpcWorld.h"
#include "Prototype.h"
#include "GameRules.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <iterator>

using namespace GameRules;

namespace
{
    constexpr WorldPoint Market{-5, 3};
    constexpr WorldPoint Training{1, 6};
    constexpr WorldPoint Workshop{-6, 5};

    double Separation(WorldPoint a, WorldPoint b)
    {
        return Length(a.x - b.x, a.y - b.y);
    }
} // namespace

NpcWorld::NpcWorld(Prototype& game, std::uint64_t seed)
    : m_Game(game),
      m_Seed(seed)
{
    for (const auto& enemy : game.m_Enemies)
    {
        m_Enemies.push_back(enemy.Id());
    }

    struct Resident
    {
        const char* name;
        NpcRole role;
        NpcGoal goal;
        WorldPoint position;
    };

    const Resident residents[] = {{"엘라", NpcRole::Priest, NpcGoal::Faith, {0, 1.6}},
                                  {"루카", NpcRole::Priest, NpcGoal::Health, {-1, 2}},
                                  {"가렌", NpcRole::Knight, NpcGoal::Power, {2, 4}},
                                  {"테오", NpcRole::Knight, NpcGoal::Faith, {1, 6}},
                                  {"리나", NpcRole::Adventurer, NpcGoal::Wisdom, {-3, 4}},
                                  {"도란", NpcRole::Adventurer, NpcGoal::Wealth, {3, 4}},
                                  {"미르", NpcRole::Adventurer, NpcGoal::Health, {-5, 5}},
                                  {"베른", NpcRole::Merchant, NpcGoal::Wealth, {-5, 3}},
                                  {"에다", NpcRole::Merchant, NpcGoal::Wisdom, {-6, 3}},
                                  {"칼로", NpcRole::Raider, NpcGoal::Wealth, {-8, 7}},
                                  {"레이크", NpcRole::Raider, NpcGoal::Power, {-7, 8}},
                                  {"재에 잠긴 자", NpcRole::Raider, NpcGoal::Power, {5, 10}}};
    const auto root = game.Scene().Spawn<Actor>(game.m_GameplayRoot, "VillageResidents").Id();
    for (size_t i = 0; i < std::size(residents); ++i)
    {
        const auto& definition = residents[i];
        NpcProfile profile;
        profile.id = i + 1;
        profile.name = definition.name;
        profile.role = definition.role;
        profile.goal = definition.goal;
        switch (profile.role)
        {
        case NpcRole::Priest:
            profile.compassion = 0.95f;
            profile.faith = 0.95f;
            profile.greed = 0.1f;
            profile.caution = 0.8f;
            break;
        case NpcRole::Knight:
            profile.courage = 0.9f;
            profile.duty = 0.95f;
            profile.greed = 0.1f;
            profile.faith = 0.8f;
            break;
        case NpcRole::Adventurer:
            profile.courage = 0.75f;
            profile.greed = 0.6f;
            profile.caution = 0.4f;
            break;
        case NpcRole::Merchant:
            profile.greed = 0.85f;
            profile.caution = 0.9f;
            profile.courage = 0.3f;
            break;
        case NpcRole::Raider:
            profile.greed = 0.95f;
            profile.compassion = 0.1f;
            profile.courage = 0.8f;
            profile.raidWeight = i == 9 ? 0.95f : 0.65f;
            profile.faith = 0.1f;
            break;
        }
        profile.temperature = 0.9f;
        if (i == 11)
        {
            profile.corruptionWeight = 0.95f;
        }
        WorldPoint origin = definition.position;
        if (!Walkable(origin.x, origin.y))
        {
            bool found = false;
            for (int radius = 1; radius <= 4 && !found; ++radius)
            {
                for (int y = -radius; y <= radius && !found; ++y)
                {
                    for (int x = -radius; x <= radius; ++x)
                    {
                        const WorldPoint point{definition.position.x + x,
                                               definition.position.y + y};
                        if (Walkable(point.x, point.y))
                        {
                            origin = point;
                            found = true;
                            break;
                        }
                    }
                }
            }
        }
        auto& npc = game.Scene().Spawn<NpcActor>(root, game, *this, profile, origin);
        npc.randomState = Hash(profile.id, 0, seed);
        npc.nextDecision = 0.15 * i;
        npc.nextSense = 0.03 * i;
        npc.vitals.gold += int(i) * 3;
        npc.vitals.strength = profile.role == NpcRole::Knight ? 15.0f : 10.0f;
        npc.vitals.holyPower = profile.role == NpcRole::Priest ? 65.0f : 20.0f;
        if (i == 6)
        {
            npc.vitals.health = 58;
            npc.vitals.corruption = 45;
        }
        if (i == 11)
        {
            npc.vitals.corruption = 90;
            npc.vitals.strength = 17;
        }
        m_Actors.push_back(npc.Id());
    }
    wchar_t executable[32768] = {};
    const DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
    if (length > 0 && length < 32768)
    {
        m_LogDirectory = std::filesystem::path(executable).parent_path() / "NpcAI";
    }
    game.m_Help = false;
    Event("성당 마을에 주민 12명이 정착했다. 성역 밖에서는 PK와 약탈이 가능하다.");
}

NpcActor* NpcWorld::Npc(NpcEntity entity) const
{
    if (entity < 101 || entity >= 101 + m_Actors.size())
    {
        return nullptr;
    }
    return dynamic_cast<NpcActor*>(m_Game.Scene().Find(m_Actors[size_t(entity - 101)]));
}

Actor* NpcWorld::Resolve(NpcEntity entity) const
{
    if (entity == PlayerEntity)
    {
        return &m_Game.Player();
    }
    if (entity >= 10000 && entity < 10000 + m_Enemies.size())
    {
        return m_Game.Scene().Find(m_Enemies[size_t(entity - 10000)]);
    }
    return Npc(entity);
}

NpcEntity NpcWorld::Entity(const Actor& actor) const
{
    if (actor.Id() == m_Game.Player().Id())
    {
        return PlayerEntity;
    }
    if (const auto* npc = dynamic_cast<const NpcActor*>(&actor))
    {
        return npc->Entity();
    }
    const auto enemy = std::find(m_Enemies.begin(), m_Enemies.end(), actor.Id());
    return enemy == m_Enemies.end() ? NoNpcEntity : 10000 + (enemy - m_Enemies.begin());
}

bool NpcWorld::Alive(NpcEntity entity) const
{
    const auto* actor = Resolve(entity);
    if (!actor || !actor->EnabledInHierarchy())
    {
        return false;
    }
    if (const auto* npc = dynamic_cast<const NpcActor*>(actor))
    {
        return npc->vitals.alive;
    }
    if (const auto* enemy = dynamic_cast<const EnemyActor*>(actor))
    {
        return enemy->alive;
    }
    return m_Game.Player().health > 0;
}

bool NpcWorld::Visible(const NpcActor& npc, NpcEntity entity) const
{
    const auto* target = Resolve(entity);
    return target && Alive(entity) && Separation(npc.WorldPosition(), target->WorldPosition()) <= 8
           && m_Game.ClearShot(npc.WorldX(), npc.WorldY(), target->WorldX(), target->WorldY());
}

bool NpcWorld::Hostile(const NpcActor& npc, NpcEntity entity) const
{
    if (entity == npc.Entity())
    {
        return false;
    }
    if (entity >= 10000 || npc.vitals.frenzy)
    {
        return true;
    }
    const auto relation = npc.relations.find(entity);
    return relation != npc.relations.end() && relation->second.grudge >= 0.6f;
}

bool NpcWorld::InVillage(WorldPoint p) const
{
    return p.x >= -10 && p.x <= 3 && p.y >= -6 && p.y <= 10;
}

void NpcWorld::Sense(NpcActor& npc)
{
    const auto observe = [&](NpcEntity id)
    {
        if (id == npc.Entity() || !Visible(npc, id))
        {
            return;
        }
        auto found = std::find_if(npc.knowledge.begin(),
                                  npc.knowledge.end(),
                                  [id](const NpcMemory& m)
                                  {
                                      return m.entity == id;
                                  });
        if (found == npc.knowledge.end())
        {
            npc.knowledge.push_back({});
            found = npc.knowledge.end() - 1;
        }
        const Actor* actor = Resolve(id);
        float ratio = 1;
        if (const auto* other = dynamic_cast<const NpcActor*>(actor))
        {
            ratio = other->vitals.health / other->vitals.maxHealth;
        }
        else if (const auto* enemy = dynamic_cast<const EnemyActor*>(actor))
        {
            ratio = enemy->health / enemy->maxHealth;
        }
        else
        {
            ratio = m_Game.Player().health / m_Game.MaxHealth();
        }
        *found = {id, actor->WorldPosition(), m_Time, ratio, Hostile(npc, id), "시야 목격"};
    };
    observe(PlayerEntity);
    for (size_t i = 0; i < m_Actors.size(); ++i)
    {
        observe(101 + i);
    }
    for (size_t i = 0; i < m_Enemies.size(); ++i)
    {
        observe(10000 + i);
    }
    npc.knowledge.erase(std::remove_if(npc.knowledge.begin(),
                                       npc.knowledge.end(),
                                       [&](const NpcMemory& memory)
                                       {
                                           return m_Time - memory.seenAt > 30;
                                       }),
                        npc.knowledge.end());
    std::sort(npc.knowledge.begin(),
              npc.knowledge.end(),
              [](const NpcMemory& a, const NpcMemory& b)
              {
                  return a.entity < b.entity;
              });
    npc.nextSense =
        m_Time + (npc.mode == NpcMode::Combat || npc.mode == NpcMode::Evacuate ? 0.3 : 0.8);
}

void NpcWorld::Update(float dt)
{
    m_Accumulator += dt;
    while (m_Accumulator >= 0.05)
    {
        m_Accumulator -= 0.05;
        m_Time += 0.05;
        for (auto it = m_Reservations.begin(); it != m_Reservations.end();)
        {
            if (it->second.until <= m_Time || !Alive(it->second.owner))
            {
                it = m_Reservations.erase(it);
            }
            else
            {
                ++it;
            }
        }
        m_Game.Scene().Update(0.05f, TickGroup::Npc);
        while (!m_Requests.empty() && m_Requests.front().expires < m_Time)
        {
            m_Game.Notice("요청 만료: 대상이 위험하거나 응답할 수 없습니다.");
            m_Requests.pop_front();
        }
    }
    if (m_Time >= m_NextExport)
    {
        m_NextExport = m_Time + 10;
        ExportLog();
    }
    const auto monitorNow = std::chrono::steady_clock::now();
    if (monitorNow >= m_NextMonitor)
    {
        m_NextMonitor = monitorNow + std::chrono::seconds(1);
        ExportMonitor();
    }
}

void NpcWorld::UpdateActor(NpcActor& npc, float dt)
{
    auto& v = npc.vitals;
    if (!v.alive)
    {
        npc.mode = NpcMode::Dead;
        Release(npc);
        return;
    }
    npc.distant = Separation(npc.WorldPosition(), m_Game.Player().WorldPosition()) > 40;
    npc.stationaryTime += dt;
    if (Separation(npc.WorldPosition(), npc.activityAnchor) > 1.0)
    {
        npc.activityAnchor = npc.WorldPosition();
        npc.stationaryTime = 0;
    }
    v.hunger = (std::min)(1.0f, v.hunger + dt * 0.002f);
    v.fatigue = (std::min)(1.0f, v.fatigue + dt * 0.001f);
    v.morale = (std::min)(1.0f, v.morale + dt * 0.001f);
    if (!InVillage(npc.WorldPosition()) && !npc.distant)
    {
        const float resistance = std::clamp(v.holyPower * 0.005f + v.wisdom * 0.002f, 0.0f, 0.75f);
        v.corruption = (std::min)(150.0f, v.corruption + dt * 0.12f * (1 - resistance));
    }
    if (!v.frenzy && v.corruption >= 85 && npc.profile.corruptionWeight >= 0.8f)
    {
        v.frenzy = true;
        Event(npc.profile.name + "의 오염 광폭화: 주변 대상을 가리지 않는다.");
        npc.nextSense = 0;
    }
    const bool important = npc.execution.phase != NpcPhase::Complete || m_Time - npc.lastHurt < 8;
    if (npc.distant && !important)
    {
        // Distant summary never rolls a kill, robbery, reward or narrative transition.
        v.fatigue = (std::max)(0.0f, v.fatigue - dt * 0.004f);
        v.health = (std::min)(v.maxHealth, v.health + dt * 0.1f);
        npc.nextDecision = m_Time + 5;
        return;
    }
    if (m_Time >= npc.nextSense)
    {
        Sense(npc);
    }
    bool threat = false;
    const bool sheltered = InsideSanctuary(npc.WorldX(), npc.WorldY());
    for (const auto& memory : npc.knowledge)
    {
        const double alertRange = m_Time - npc.lastHurt < 6 ? 8.0 : 5.5;
        if (!sheltered && memory.hostile && m_Time - memory.seenAt < 1.5
            && Separation(npc.WorldPosition(), memory.position) < alertRange
            && !InsideSanctuary(memory.position.x, memory.position.y))
        {
            threat = true;
        }
    }
    const auto oldMode = npc.mode;
    npc.mode = threat ? (v.health < v.maxHealth * 0.28f || v.morale < 0.22f ? NpcMode::Evacuate
                                                                            : NpcMode::Combat)
                      : NpcMode::Daily;
    if (!threat && npc.execution.playerRequest && npc.execution.phase != NpcPhase::Complete)
    {
        npc.mode = NpcMode::Interaction;
    }
    if (threat)
    {
        npc.calmSince = m_Time;
    }
    const auto action = npc.execution.action;
    const bool lockedAttack = action == NpcAction::Attack || action == NpcAction::Raid;
    if (threat && !lockedAttack && npc.execution.phase != NpcPhase::Complete
        && action != NpcAction::Flee && action != NpcAction::Heal && action != NpcAction::Guard
        && action != NpcAction::Observe && action != NpcAction::Bless)
    {
        Finish(npc, false, "관측한 위협으로 중단");
    }
    if (oldMode != npc.mode)
    {
        npc.nextDecision = (std::min)(npc.nextDecision, m_Time);
    }
    Execute(npc, dt);
    if (npc.execution.phase == NpcPhase::Complete && m_Time >= npc.nextDecision)
    {
        Decide(npc);
    }
}

bool NpcWorld::CanReserve(std::uint64_t resource, NpcEntity owner) const
{
    const auto found = m_Reservations.find(resource);
    return resource == 0 || found == m_Reservations.end() || found->second.until <= m_Time
           || found->second.owner == owner;
}

std::uint64_t NpcWorld::Resource(NpcAction action, NpcEntity target) const
{
    if (action == NpcAction::Heal || action == NpcAction::Bless)
    {
        return 100000 + target;
    }
    if (action == NpcAction::Trade)
    {
        return 200001;
    }
    if (action == NpcAction::Work)
    {
        return 200002;
    }
    if (action == NpcAction::Train)
    {
        return 200003;
    }
    if (action == NpcAction::Purify)
    {
        return 200004;
    }
    return 0;
}

void NpcWorld::Release(NpcActor& npc)
{
    const auto found = m_Reservations.find(npc.execution.reservation);
    if (found != m_Reservations.end() && found->second.owner == npc.Entity())
    {
        m_Reservations.erase(found);
    }
    npc.execution.reservation = 0;
}

WorldPoint NpcWorld::ActivityPoint(const NpcActor& npc, NpcAction action) const
{
    const auto visit = npc.routeVisits[static_cast<size_t>(action)];
    const size_t offset = static_cast<size_t>(npc.profile.id + visit);
    // These are known places, never hidden live target coordinates.
    std::array<WorldPoint, 6> places{};
    if (action == NpcAction::Hunt)
    {
        places = {{{7, 4}, {11, 4}, {15, 4}, {19, 4}, {16, 8}, {8, 8}}};
    }
    else if (action == NpcAction::Patrol)
    {
        switch (npc.profile.role)
        {
        case NpcRole::Priest:
            places = {{{-5, 2}, {-6, 6}, {-2, 7}, {2, 4}, {-1, -1}, {-6, -1}}};
            break;
        case NpcRole::Knight:
            places = {{{3, 4}, {3, 9}, {-4, 9}, {-9, 2}, {-6, -2}, {3, -2}}};
            break;
        case NpcRole::Adventurer:
            places = {{{3, 4}, {7, 4}, {11, 4}, {4, 9}, {-2, 7}, {-5, 2}}};
            break;
        case NpcRole::Raider:
            places = {{{-9, 8}, {-5, 9}, {3, 10}, {7, 4}, {4, -2}, {-9, 1}}};
            break;
        case NpcRole::Merchant:
            places = {{{-5, 1}, {-6, 5}, {-2, 7}, {2, 5}, {-6, -2}, {-9, 2}}};
            break;
        }
    }
    else
    {
        const WorldPoint center = action == NpcAction::Work    ? Workshop
                                  : action == NpcAction::Trade ? Market
                                  : action == NpcAction::Train ? Training
                                                               : WorldPoint{0, 0};
        const bool sacred = action == NpcAction::Pray || action == NpcAction::Purify
                            || action == NpcAction::Flee || action == NpcAction::Reflect;
        for (size_t i = 0; i < places.size(); ++i)
        {
            const double angle = (double(npc.profile.id) + i * 2.0) * 2.39996323;
            const double radius = sacred ? 1.9 : 0.85;
            places[i] = {center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius};
        }
    }
    const bool roaming = action == NpcAction::Patrol || action == NpcAction::Hunt;
    for (size_t i = 0; i < places.size(); ++i)
    {
        const auto point = places[(offset + i) % places.size()];
        if (Walkable(point.x, point.y) && (!roaming || Separation(npc.WorldPosition(), point) > 2))
        {
            return point;
        }
    }
    return npc.home;
}

void NpcWorld::BuildCandidates(NpcActor& npc)
{
    npc.candidates.clear();
    const auto& v = npc.vitals;
    const auto role = npc.profile.role;
    const bool combat = npc.mode == NpcMode::Combat || npc.mode == NpcMode::Evacuate;
    NpcEntity foe = 0;
    NpcEntity victim = 0;
    NpcEntity patient = v.health < v.maxHealth * 0.8f ? npc.Entity() : 0;
    WorldPoint foePoint;
    WorldPoint victimPoint;
    WorldPoint patientPoint = npc.WorldPosition();
    double foeDistance = 100;
    double victimDistance = 100;
    float patientRatio = patient ? v.health / v.maxHealth : 0.85f;
    for (const auto& memory : npc.knowledge)
    {
        if (m_Time - memory.seenAt > 1.5)
        {
            continue;
        }
        const double distance = Separation(npc.WorldPosition(), memory.position);
        if (memory.hostile && distance < foeDistance
            && !InsideSanctuary(memory.position.x, memory.position.y))
        {
            foe = memory.entity;
            foePoint = memory.position;
            foeDistance = distance;
        }
        if (memory.entity < 10000 && memory.entity != npc.Entity() && distance < victimDistance
            && !InsideSanctuary(memory.position.x, memory.position.y))
        {
            victim = memory.entity;
            victimPoint = memory.position;
            victimDistance = distance;
        }
        if (!memory.hostile && memory.healthRatio < patientRatio && role == NpcRole::Priest)
        {
            patient = memory.entity;
            patientPoint = memory.position;
            patientRatio = memory.healthRatio;
        }
    }
    for (size_t index = 0; index < NpcActionCount; ++index)
    {
        NpcCandidate c;
        c.action = static_cast<NpcAction>(index);
        c.destination = npc.WorldPosition();
        c.allowed = true;
        c.base = 10;
        const auto require = [&](bool condition, const char* reason)
        {
            if (!condition && c.allowed)
            {
                c.allowed = false;
                c.reason = reason;
            }
        };
        switch (c.action)
        {
        case NpcAction::Observe:
            c.base = 5;
            break;
        case NpcAction::Work:
            c.base = 25;
            c.destination = ActivityPoint(npc, c.action);
            require(!combat && m_Treasury >= 5 && v.fatigue < 0.7f
                        && (role == NpcRole::Merchant || role == NpcRole::Adventurer),
                    "생산 직업/작업 예산/피로 여유 필요");
            break;
        case NpcAction::Rest:
            c.destination = npc.home;
            require(!combat && (v.fatigue > 0.35f || v.health < v.maxHealth * 0.8f),
                    "휴식할 부상/피로 필요");
            break;
        case NpcAction::Eat:
            require(!combat && v.food > 0 && v.hunger > 0.2f, "식량/허기/안전 필요");
            break;
        case NpcAction::Patrol:
            c.base = 35;
            c.destination = ActivityPoint(npc, c.action);
            require(!combat, "위험 대응 후 직업별 순회");
            break;
        case NpcAction::Hunt:
            c.base = 40;
            c.destination = ActivityPoint(npc, c.action);
            require(!combat && (role == NpcRole::Knight || role == NpcRole::Adventurer),
                    "토벌 직업/일상 상태 필요");
            break;
        case NpcAction::Raid:
            c.base = 20;
            c.target = victim;
            c.destination = victimPoint;
            require(role == NpcRole::Raider && npc.profile.raidWeight > 0 && victim,
                    "약탈 역할/목격한 피해자 필요");
            break;
        case NpcAction::Trade:
            c.base = 15;
            c.destination = ActivityPoint(npc, c.action);
            require(!combat
                        && ((v.goods > 0 && m_Treasury >= 8)
                            || (v.gold >= 6 && m_MarketFood > 0 && v.food < 3)
                            || (v.gold >= 12 && m_MarketMedicine > 0 && v.medicine < 2)),
                    "거래할 재고/돈/안전 필요");
            break;
        case NpcAction::Bless:
            c.base = 20;
            c.target = patient;
            c.destination = patientPoint;
            require(role == NpcRole::Priest && patient && v.holyPower >= 8,
                    "성직자/부상자/성력 8 필요");
            break;
        case NpcAction::Pray:
            c.base = 20;
            c.destination = ActivityPoint(npc, c.action);
            require(!combat && v.holyPower < 80 && (npc.profile.faith >= 0.7f || v.holyPower < 25),
                    "안전/신앙 또는 부족한 성력 필요");
            break;
        case NpcAction::Train:
            c.base = 18;
            c.destination = ActivityPoint(npc, c.action);
            require(!combat && v.fatigue < 0.8f && role != NpcRole::Priest,
                    "훈련 가능한 직업/체력 필요");
            break;
        case NpcAction::Reflect:
            c.base = 15;
            c.destination = ActivityPoint(npc, c.action);
            require(!combat && v.wisdom < 100, "안전/깨달음 성장 여유 필요");
            break;
        case NpcAction::Heal:
            c.base = 20;
            c.target = patient;
            c.destination = patientPoint;
            require(patient && v.medicine > 0, "부상자/치료제 필요");
            break;
        case NpcAction::Purify:
            c.base = 15;
            c.destination = ActivityPoint(npc, c.action);
            require(!combat && v.corruption >= 20 && v.corruption < MutationThreshold
                        && v.gold >= 3,
                    "안전/변이 이전 오염/기부금 3 필요");
            break;
        case NpcAction::DawnOil:
            c.base = 30;
            require(v.corruption >= MutationThreshold && v.dawnOil > 0, "괴물화/새벽의 성유 필요");
            break;
        case NpcAction::Flee:
            c.destination = ActivityPoint(npc, c.action);
            require(combat
                        && (npc.mode == NpcMode::Evacuate
                            || (role == NpcRole::Priest && v.holyPower < 4)
                            || (role == NpcRole::Merchant && v.health < v.maxHealth * 0.6f)),
                    "대피가 필요한 부상/자원 고갈 없음");
            break;
        case NpcAction::Attack:
            c.base = 60;
            c.target = foe;
            c.destination = foePoint;
            require(foe && (role != NpcRole::Priest || v.holyPower >= 4),
                    "관측한 적대 대상/공격 능력 필요");
            break;
        case NpcAction::Guard:
            c.base = 5;
            require(combat, "실제 위험이 있을 때만 제자리 경계");
            break;
        case NpcAction::ReturnHome:
            c.base = 20;
            c.destination = npc.home;
            require(!combat && Separation(npc.WorldPosition(), npc.home) > 4
                        && (v.fatigue > 0.55f || v.health < v.maxHealth * 0.7f),
                    "귀환 필요 없음/위험 중");
            break;
        default:
            require(false, "등록되지 않은 행동");
            break;
        }
        if (npc.mode == NpcMode::Evacuate)
        {
            require(c.action == NpcAction::Flee || c.action == NpcAction::Heal
                        || c.action == NpcAction::Guard,
                    "낮은 체력: 생존 행동 우선");
        }
        require(m_Time >= npc.cooldown[index], "쿨다운");
        const auto failed = npc.failedUntil.find({c.action, c.target});
        require(failed == npc.failedUntil.end() || failed->second <= m_Time,
                "실패한 행동/대상 대기 3초");
        require(CanReserve(Resource(c.action, c.target), npc.Entity()), "시설/대상 예약 경쟁");
        if (c.allowed && !BuildPath(npc, c.destination))
        {
            c.allowed = false;
            c.reason = "접근 경로 없음";
            npc.failedUntil[{c.action, c.target}] = m_Time + 3;
        }
        c.cost = float((std::min)(20.0, Separation(npc.WorldPosition(), c.destination) * 1.5));
        if (c.allowed)
        {
            c.reason = "실행 조건 통과";
        }
        npc.candidates.push_back(c);
    }
    std::sort(npc.candidates.begin(),
              npc.candidates.end(),
              [](const auto& a, const auto& b)
              {
                  return std::string(NpcActionId(a.action)) < NpcActionId(b.action);
              });
}

void NpcWorld::Decide(NpcActor& npc)
{
    ++npc.decisions;
    BuildCandidates(npc);
    NpcBrain::Score(npc, m_Time);
    // Explicit player requests precede the ordinary action lottery.
    for (auto it = m_Requests.begin(); it != m_Requests.end(); ++it)
    {
        if (it->npc == npc.Entity() && it->expires >= m_Time && npc.mode != NpcMode::Combat
            && npc.mode != NpcMode::Evacuate)
        {
            NpcCandidate request;
            request.allowed = true;
            request.action = it->action;
            request.target = PlayerEntity;
            request.destination = m_Game.Player().WorldPosition();
            const bool started = Start(npc, request, true);
            m_Requests.erase(it);
            if (started)
            {
                LogDecision(npc);
                return;
            }
            m_Game.Notice("요청 거절: 경로/예약/자격을 확인하세요.");
            break;
        }
    }
    size_t selected = NpcBrain::Choose(npc);
    bool started = selected < npc.candidates.size() && Start(npc, npc.candidates[selected]);
    if (!started)
    {
        // One deterministic alternate, no repeated random draws or unbounded retry.
        auto alternate =
            std::find_if(npc.candidates.begin(),
                         npc.candidates.end(),
                         [&](const auto& c)
                         {
                             return c.allowed
                                    && c.action
                                           == (npc.mode == NpcMode::Evacuate ? NpcAction::Flee
                                                                             : NpcAction::Guard);
                         });
        started = alternate != npc.candidates.end() && Start(npc, *alternate);
    }
    if (!started)
    {
        NpcCandidate wait;
        wait.allowed = true;
        wait.destination = npc.WorldPosition();
        Start(npc, wait);
    }
    LogDecision(npc);
}

void NpcWorld::Event(const std::string& text)
{
    m_Events.push_back(text);
    if (m_Events.size() > 5)
    {
        m_Events.pop_front();
    }
    m_Log.push_back("t=" + std::to_string(m_Time) + " event=" + text);
    while (m_Log.size() > 512)
    {
        m_Log.pop_front();
    }
}

void NpcWorld::LogDecision(const NpcActor& npc)
{
    std::ostringstream line;
    line << std::fixed << std::setprecision(3) << "t=" << m_Time << " npc=" << npc.profile.id
         << " seq=" << npc.decisions << " rng=" << npc.randomState << " u=" << npc.lastRoll
         << " state=" << NpcModeName(npc.mode) << " action=" << NpcActionId(npc.execution.action)
         << " target=" << npc.execution.target << " execution=" << npc.execution.id
         << " request=" << npc.execution.playerRequest << " next=" << npc.nextDecision
         << " destination=" << npc.execution.destination.x << ',' << npc.execution.destination.y
         << " stationary=" << npc.stationaryTime << " distance=" << npc.distanceTravelled;
    for (const auto& c : npc.candidates)
    {
        line << "\n  " << NpcActionId(c.action) << " target=" << c.target
             << " allowed=" << c.allowed << " reason=" << c.reason << " B=" << c.base
             << " P=" << c.personality << " N=" << c.need << " R=" << c.relation
             << " S=" << c.situation << " G=" << c.goal << " C=" << c.cost
             << " continuity=" << c.continuity << " repeat=" << c.repetition << " W=" << c.weight
             << " probability=" << c.probability;
    }
    m_Log.push_back(line.str());
    while (m_Log.size() > 512)
    {
        m_Log.pop_front();
    }
}

bool NpcWorld::ExportLog()
{
    if (m_LogDirectory.empty())
    {
        return false;
    }
    std::error_code error;
    std::filesystem::create_directories(m_LogDirectory, error);
    const auto temporary = m_LogDirectory / "latest.log.tmp";
    std::ofstream file(temporary, std::ios::trunc);
    if (error || !file)
    {
        if (!m_LogWarning)
        {
            std::cerr << "[NpcAI] Cannot write diagnostic snapshot.\n";
            m_LogWarning = true;
        }
        return false;
    }
    file << "NPC AI schema=2 seed=" << m_Seed << " fixed_step=0.05 time=" << m_Time
         << " treasury=" << m_Treasury << " food=" << m_MarketFood
         << " medicine=" << m_MarketMedicine << " goods=" << m_MarketGoods
         << "\nBounded diagnostic snapshot; not a loadable save.\n";
    for (const auto actor : m_Actors)
    {
        const auto* n = dynamic_cast<const NpcActor*>(m_Game.Scene().Find(actor));
        if (!n)
        {
            continue;
        }
        file << "NPC " << n->profile.id << ' ' << n->profile.name
             << " role=" << NpcRoleName(n->profile.role) << " goal=" << NpcGoalName(n->profile.goal)
             << " position=" << n->WorldX() << ',' << n->WorldY() << " hp=" << n->vitals.health
             << '/' << n->vitals.maxHealth << " gold=" << n->vitals.gold
             << " holy=" << n->vitals.holyPower << " strength=" << n->vitals.strength
             << " wisdom=" << n->vitals.wisdom << " corruption=" << n->vitals.corruption
             << " raidWeight=" << n->profile.raidWeight
             << " corruptionWeight=" << n->profile.corruptionWeight << " rng=" << n->randomState
             << " seq=" << n->decisions << " alive=" << n->vitals.alive
             << " phase=" << NpcPhaseName(n->execution.phase) << " effect=" << n->execution.applied
             << " reservation=" << n->execution.reservation << " distant=" << n->distant
             << " weapon=" << NpcWeaponFor(n->profile.role).name
             << " stationary=" << n->stationaryTime << " distance=" << n->distanceTravelled
             << " completed=" << n->completedActions << " failed=" << n->failedActions
             << " destination=" << n->execution.destination.x << ',' << n->execution.destination.y
             << '\n';
        for (const auto& memory : n->knowledge)
        {
            file << "  knowledge " << memory.entity << " at=" << memory.seenAt
                 << " source=" << memory.source << " last=" << memory.position.x << ','
                 << memory.position.y << " hostile=" << memory.hostile << '\n';
        }
        for (const auto& relation : n->relations)
        {
            file << "  relation " << relation.first << " trust=" << relation.second.trust
                 << " grudge=" << relation.second.grudge << '\n';
        }
        for (size_t i = 0; i < NpcActionCount; ++i)
        {
            if (n->cooldown[i] > m_Time)
            {
                file << "  cooldown " << NpcActionId(static_cast<NpcAction>(i)) << '='
                     << n->cooldown[i] - m_Time << '\n';
            }
        }
    }
    for (const auto& line : m_Log)
    {
        file << line << '\n';
    }
    file.close();
    return bool(file)
           && MoveFileExW(temporary.c_str(),
                          (m_LogDirectory / "latest.log").c_str(),
                          MOVEFILE_REPLACE_EXISTING);
}
