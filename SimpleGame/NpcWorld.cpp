#include "stdafx.h"
#include "NpcWorld.h"
#include "Prototype.h"
#include "GameRules.h"
#include "Outposts.h"
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

    // Shared culture defines the initial community; roles and small seeded variations
    // keep residents distinct. Identity remains stable when they leave their village.
    struct Culture
    {
        NpcGoal goal;
        float compassion;
        float duty;
        float greed;
        float faith;
        float courage;
        float raidWeight;
        std::array<NpcRole, 8> roles;
    };

    using Role = NpcRole;
    const std::array<Culture, 6> cultures = {{{NpcGoal::Health,
                                               .95f,
                                               .8f,
                                               .15f,
                                               .75f,
                                               .6f,
                                               0,
                                               {Role::Priest,
                                                Role::Priest,
                                                Role::Knight,
                                                Role::Knight,
                                                Role::Adventurer,
                                                Role::Adventurer,
                                                Role::Adventurer,
                                                Role::Merchant}},
                                              {NpcGoal::Faith,
                                               .85f,
                                               .85f,
                                               .1f,
                                               .98f,
                                               .55f,
                                               0,
                                               {Role::Priest,
                                                Role::Priest,
                                                Role::Priest,
                                                Role::Priest,
                                                Role::Knight,
                                                Role::Knight,
                                                Role::Adventurer,
                                                Role::Merchant}},
                                              {NpcGoal::Power,
                                               .65f,
                                               .98f,
                                               .15f,
                                               .7f,
                                               .95f,
                                               0,
                                               {Role::Knight,
                                                Role::Knight,
                                                Role::Knight,
                                                Role::Knight,
                                                Role::Knight,
                                                Role::Priest,
                                                Role::Adventurer,
                                                Role::Merchant}},
                                              {NpcGoal::Wealth,
                                               .5f,
                                               .5f,
                                               .9f,
                                               .3f,
                                               .4f,
                                               0,
                                               {Role::Merchant,
                                                Role::Merchant,
                                                Role::Merchant,
                                                Role::Merchant,
                                                Role::Merchant,
                                                Role::Knight,
                                                Role::Adventurer,
                                                Role::Priest}},
                                              {NpcGoal::Wisdom,
                                               .65f,
                                               .45f,
                                               .4f,
                                               .4f,
                                               .8f,
                                               0,
                                               {Role::Adventurer,
                                                Role::Adventurer,
                                                Role::Adventurer,
                                                Role::Adventurer,
                                                Role::Adventurer,
                                                Role::Priest,
                                                Role::Knight,
                                                Role::Merchant}},
                                              {NpcGoal::Wealth,
                                               .1f,
                                               .15f,
                                               .95f,
                                               .1f,
                                               .85f,
                                               .85f,
                                               {Role::Raider,
                                                Role::Raider,
                                                Role::Raider,
                                                Role::Raider,
                                                Role::Raider,
                                                Role::Raider,
                                                Role::Adventurer,
                                                Role::Merchant}}}};
    const char* names[][8] = {
        {"엘라", "루카", "가렌", "테오", "리나", "도란", "미르", "베른"},
        {"아델", "이사", "세라", "노엘", "오웬", "루벤", "하엘", "에다"},
        {"발렌", "로칸", "에릭", "브란", "헤르만", "실라", "카인", "토린"},
        {"마르타", "기욤", "로사", "니콜", "바실", "울프", "레온", "안나"},
        {"에린", "소렌", "나디아", "펠릭스", "테린", "아벨", "시온", "로엔"},
        {"칼로", "레이크", "모르", "드렉", "베카", "재에 잠긴 자", "키라", "벨트"}};
    for (size_t village = 0; village < Villages.size(); ++village)
    {
        const auto& definition = Villages[village];
        const auto& culture = cultures[village];
        const auto root = game.Scene().Spawn<Actor>(game.m_GameplayRoot, definition.name).Id();
        m_VillageRoots[village] = root;
        for (size_t resident = 0; resident < VillageInitialPopulation; ++resident)
        {
            NpcProfile profile;
            profile.id = m_Actors.size() + 1;
            profile.village = village;
            profile.name = names[village][resident % 8];
            if (resident >= 8)
            {
                profile.name += " " + std::to_string(resident / 8 + 1) + "세대";
            }
            profile.role = culture.roles[resident % 8];
            profile.goal = culture.goal;
            const float variation = (int(Hash(profile.id, 1, seed) % 9) - 4) * .01f;
            const auto trait = [variation](float value)
            {
                return std::clamp(value + variation, 0.0f, 1.0f);
            };
            profile.compassion = trait(culture.compassion);
            profile.duty = trait(culture.duty);
            profile.greed = trait(culture.greed);
            profile.faith = trait(culture.faith);
            profile.courage = trait(culture.courage);
            profile.caution = profile.role == Role::Merchant ? .9f : .5f;
            profile.raidWeight = culture.raidWeight > 0 ? trait(culture.raidWeight) : 0;
            profile.corruptionWeight = village == 5 ? (resident % 8 == 5 ? .95f : .65f) : .1f;
            profile.temperature = .9f;
            const WorldPoint home = VillageHome(village, resident);
            WorldPoint origin = home;
            if (!Walkable(origin.x, origin.y))
            {
                bool found = false;
                for (int radius = 1; radius <= 4 && !found; ++radius)
                {
                    for (int y = -radius; y <= radius && !found; ++y)
                    {
                        for (int x = -radius; x <= radius; ++x)
                        {
                            const WorldPoint point{home.x + x, home.y + y};
                            if (definition.Contains(point) && Walkable(point.x, point.y))
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
            npc.nextDecision = .15 * resident + .07 * village;
            npc.nextSense = .025 * (profile.id % 40);
            npc.nextMigration = 25 + (profile.id % 6) * 3;
            npc.vitals.gold += int(resident % 8) * 3;
            npc.birth.gold = npc.vitals.gold;
            if (village == 0 && resident == 6)
            {
                npc.vitals.health = 58;
                npc.vitals.corruption = 45;
            }
            if (village == 5)
            {
                npc.vitals.corruption = 25 + float(resident % 8) * 3;
            }
            m_Actors.push_back(npc.Id());
        }
        Event(std::string(definition.name) + "에 주민 " + std::to_string(VillageInitialPopulation)
              + "명이 정착했다: " + definition.culture);
    }
    wchar_t executable[32768] = {};
    const DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
    if (length > 0 && length < 32768)
    {
        m_LogDirectory = std::filesystem::path(executable).parent_path() / "NpcAI";
    }
    game.m_Help = false;
    RebuildNpcSpatialIndex();
    InitializeSociety();
    InitializeOutposts();
    std::cout << "[WorldScale] villages=" << Villages.size()
              << " populationPerVillage=" << VillageInitialPopulation
              << " totalNpcs=" << m_Actors.size() << " villageExtent=" << VillageHalfExtent * 2
              << " centerSpacing=" << ShrineSpacing << " roadGap=" << VillageRoadGap
              << " walkingSeconds=" << VillageRoadGap / PlayerWalkSpeed
              << " pigeonSpeed=" << CarrierPigeonSpeed << " outposts=" << Outposts.size()
              << " outpostExtent=" << OutpostHalfExtent * 2
              << " outpostCapacity=" << OutpostCapacity << '\n';
    Event("6개 마을에 주민 " + std::to_string(m_Actors.size())
          + "명이 정착했다. 마을 사이 일반 걷기 약 30초, 전서구 통신과 이주가 시작된다.");
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
    return VillageAt(p) >= 0;
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
    for (auto id : NearbyNpcs(npc.WorldPosition(), 9))
    {
        const auto* other = static_cast<const NpcActor*>(m_Game.Scene().Find(id));
        if (other)
        {
            observe(other->Entity());
        }
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
    npc.nextSense = m_Time
                    + (npc.mode == NpcMode::Combat || npc.mode == NpcMode::Evacuate ? 0.3
                       : npc.distant                                                ? 1.2
                                                                                    : 0.8);
}

void NpcWorld::Update(float dt)
{
    m_Performance = {};
    m_Accumulator += dt;
    while (m_Accumulator >= 0.05)
    {
        m_Accumulator -= 0.05;
        m_Time += 0.05;
        RebuildNpcSpatialIndex();
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
        if (m_Time >= m_NextWorldSimulation)
        {
            m_NextWorldSimulation = m_Time + 1;
            UpdateOutposts();
            UpdateSociety();
            if (m_Time >= m_NextMigrationReview)
            {
                m_NextMigrationReview = m_Time + 10;
                UpdateMigrationChoices();
            }
        }
        UpdateCommunication();
        for (auto& camp : m_Outposts)
        {
            for (auto id : camp.members)
            {
                auto* enemy = dynamic_cast<EnemyActor*>(m_Game.Scene().Find(id));
                if (enemy && enemy->EnabledInHierarchy())
                {
                    UpdateOutpostEnemy(*enemy, 0.05f);
                }
            }
        }
        m_Game.Scene().Update(0.05f, TickGroup::Npc);
        if (m_Time >= m_NextRumor)
        {
            m_NextRumor = m_Time + 3;
            SpreadRumors();
        }
        while (!m_Requests.empty() && m_Requests.front().expires < m_Time)
        {
            m_Game.Notice("요청 만료: 대상이 위험하거나 응답할 수 없습니다.");
            m_Requests.pop_front();
        }
    }
    const auto exportStart = std::chrono::steady_clock::now();
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
    m_Performance.exportMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - exportStart)
            .count();
}

void NpcWorld::UpdateActor(NpcActor& npc, float dt)
{
    auto& v = npc.vitals;
    if (!v.alive)
    {
        if (npc.mission.type != VillageMission::None)
        {
            RecordWorldEvent(npc.mission.type == VillageMission::Relocate ? "migration_failed"
                                                                          : "expedition_failed",
                             npc.profile.name + "의 원정이 사망으로 종료되었다",
                             npc.WorldPosition(),
                             &npc);
            npc.mission = {};
        }
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
    if (!InVillage(npc.WorldPosition()))
    {
        const float resistance = std::clamp(v.holyPower * 0.005f + v.wisdom * 0.002f, 0.0f, 0.75f);
        v.corruption = (std::min)(150.0f, v.corruption + dt * 0.12f * (1 - resistance));
    }
    if (!v.frenzy && v.corruption >= 85 && npc.profile.corruptionWeight >= 0.8f)
    {
        v.frenzy = true;
        Event(npc.profile.name + "의 오염 광폭화: 주변 대상을 가리지 않는다.");
        RecordWorldEvent("frenzy", npc.profile.name + "의 오염 광폭화", npc.WorldPosition(), &npc);
        npc.nextSense = 0;
    }
    // All villages keep their real action simulation even outside streamed chunks.
    // Only perception is less frequent at a distance; movement retains its fixed step.
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
    if (UpdateMission(npc, dt))
    {
        return;
    }
    Execute(npc, dt);
    if (npc.execution.phase == NpcPhase::Complete && m_Time >= npc.nextDecision
        && (npc.mission.type == VillageMission::None || npc.mode != NpcMode::Daily))
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

std::uint64_t NpcWorld::Resource(const NpcActor& npc, NpcAction action, NpcEntity target) const
{
    if (action == NpcAction::Heal || action == NpcAction::Bless)
    {
        return 100000 + target;
    }
    if (action == NpcAction::Trade)
    {
        const int location = VillageAt(npc.WorldPosition());
        return 200000 + (location >= 0 ? size_t(location) : npc.profile.village) * 64
               + (npc.profile.id % 4);
    }
    if (action == NpcAction::Work)
    {
        return 200000 + npc.profile.village * 64 + 8 + npc.profile.id % 4;
    }
    if (action == NpcAction::Train)
    {
        return 200000 + npc.profile.village * 64 + 16 + npc.profile.id % 4;
    }
    if (action == NpcAction::Purify)
    {
        return 200000 + npc.profile.village * 64 + 24 + npc.profile.id % 4;
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
        places = {{{24, 0}, {28, 4}, {34, 4}, {24, 12}, {-24, 0}, {0, 24}}};
    }
    else if (action == NpcAction::Patrol)
    {
        switch (npc.profile.role)
        {
        case NpcRole::Priest:
            places = {{{-12, 4}, {-4, 12}, {4, 12}, {12, 4}, {-4, -12}, {4, -12}}};
            break;
        case NpcRole::Knight:
            places = {{{0, 17}, {17, 0}, {0, -17}, {-17, 0}, {-12, -12}, {12, 12}}};
            break;
        case NpcRole::Adventurer:
            places = {{{20, 0}, {0, 20}, {-20, 0}, {0, -20}, {12, 12}, {-12, -12}}};
            break;
        case NpcRole::Raider:
            places = {{{24, 0}, {0, 24}, {-24, 0}, {0, -24}, {16, 12}, {-16, -12}}};
            break;
        case NpcRole::Merchant:
            places = {{{-6, 4}, {-10, 4}, {6, 4}, {10, 4}, {-12, -4}, {12, -4}}};
            break;
        }
    }
    else
    {
        constexpr std::array<WorldPoint, 4> markets = {{{-6, 4}, {-10, 4}, {6, 4}, {10, 4}}};
        constexpr std::array<WorldPoint, 4> workshops = {{{-12, -8}, {12, 8}, {-12, 8}, {12, -8}}};
        constexpr std::array<WorldPoint, 4> training = {{{0, 12}, {0, -12}, {12, 0}, {-12, 0}}};
        const size_t lane = npc.profile.id % 4;
        const WorldPoint center = action == NpcAction::Work    ? workshops[lane]
                                  : action == NpcAction::Trade ? markets[lane]
                                  : action == NpcAction::Train ? training[lane]
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
    const auto center = Villages[npc.profile.village].center;
    for (auto& point : places)
    {
        point.x += center.x;
        point.y += center.y;
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
    const auto& economy = m_Economies[npc.profile.village];
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
        const auto* other = Npc(memory.entity);
        const bool sameVillage = other && other->profile.village == npc.profile.village;
        if ((!sameVillage || memory.hostile) && memory.entity < 10000
            && memory.entity != npc.Entity() && distance < victimDistance
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
            require(!combat && economy.treasury >= 5 && v.fatigue < 0.7f
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
                        && ((v.goods > 0 && economy.treasury >= 8)
                            || (v.gold >= 6 && economy.food > 0 && v.food < 3)
                            || (v.gold >= 12 && economy.medicine > 0 && v.medicine < 2)),
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
        require(CanReserve(Resource(npc, c.action, c.target), npc.Entity()), "시설/대상 예약 경쟁");
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
         << " village=" << Villages[npc.profile.village].id << " seq=" << npc.decisions
         << " rng=" << npc.randomState << " u=" << npc.lastRoll
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
    file << "NPC AI schema=4 seed=" << m_Seed << " fixed_step=0.05 time=" << m_Time
         << " villages=" << Villages.size()
         << "\nBounded diagnostic snapshot; not a loadable save.\n";
    file << "WORLD_SCALE villageHalfExtent=" << VillageHalfExtent
         << " initialResidents=" << VillageInitialPopulation << " totalNpcs=" << m_Actors.size()
         << " roadGap=" << VillageRoadGap << " spacing=" << ShrineSpacing
         << " walkingSeconds=" << VillageRoadGap / PlayerWalkSpeed
         << " pigeonSpeed=" << CarrierPigeonSpeed << " migrationFloor=" << VillageMigrationFloor
         << " populationCapacity=" << VillagePopulationCapacity
         << " spatialBuckets=" << m_NpcSpatialIndex.size() << '\n';
    for (size_t i = 0; i < Villages.size(); ++i)
    {
        const auto& economy = m_Economies[i];
        file << "VILLAGE " << Villages[i].id << " name=" << Villages[i].name
             << " culture=" << Villages[i].culture << " center=" << Villages[i].center.x << ','
             << Villages[i].center.y << " treasury=" << economy.treasury << " food=" << economy.food
             << " medicine=" << economy.medicine << " goods=" << economy.goods << '\n';
    }
    for (const auto actor : m_Actors)
    {
        const auto* n = dynamic_cast<const NpcActor*>(m_Game.Scene().Find(actor));
        if (!n)
        {
            continue;
        }
        file << "NPC " << n->profile.id << ' ' << n->profile.name
             << " village=" << Villages[n->profile.village].id
             << " role=" << NpcRoleName(n->profile.role) << " goal=" << NpcGoalName(n->profile.goal)
             << " position=" << n->WorldX() << ',' << n->WorldY() << " hp=" << n->vitals.health
             << '/' << n->vitals.maxHealth << " gold=" << n->vitals.gold
             << " holy=" << n->vitals.holyPower << " strength=" << n->vitals.strength
             << " armor=" << n->vitals.armor << " moveSpeed=" << n->vitals.moveSpeed
             << " birthHp=" << n->birth.health << " birthStrength=" << n->birth.strength
             << " mission=" << VillageMissionName(n->mission.type)
             << " missionVillage=" << Villages[n->mission.destination].id
             << " returning=" << n->mission.returning << " missionDeadline=" << n->mission.deadline
             << " wisdom=" << n->vitals.wisdom << " corruption=" << n->vitals.corruption
             << " raidWeight=" << n->profile.raidWeight
             << " corruptionWeight=" << n->profile.corruptionWeight << " rng=" << n->randomState
             << " seq=" << n->decisions << " alive=" << n->vitals.alive
             << " phase=" << NpcPhaseName(n->execution.phase) << " effect=" << n->execution.applied
             << " reservation=" << n->execution.reservation << " distant=" << n->distant
             << " weapon=" << NpcWeaponFor(n->profile.role).name
             << " stationary=" << n->stationaryTime << " distance=" << n->distanceTravelled
             << " migrations=" << n->migrations << " migrationReason=" << n->migrationReason
             << " migrationHome=" << n->migrationHome.x << ',' << n->migrationHome.y
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
    for (size_t village = 0; village < m_Societies.size(); ++village)
    {
        const auto& society = m_Societies[village];
        file << "SOCIETY village=" << Villages[village].id << " alignment=" << society.alignment
             << " threatened=" << society.threatened << " dispatches=" << society.dispatches
             << " nextDispatch=" << society.nextDispatch << " postsSent=" << society.postsSent
             << " postsReceived=" << society.postsReceived << " immigrants=" << society.immigrants
             << " emigrants=" << society.emigrants << '\n';
        for (const auto& item : society.known)
        {
            file << "  villageKnowledge target=" << Villages[item.first].id
                 << " observedAt=" << item.second.observedAt
                 << " alignment=" << item.second.alignment << " position=" << item.second.position.x
                 << ',' << item.second.position.y
                 << " source=" << Villages[item.second.sourceVillage].id
                 << " geographicRisk=" << item.second.geographicRisk
                 << " relation=" << society.relations[item.first] << '\n';
        }
    }
    for (size_t i = 0; i < m_Outposts.size(); ++i)
    {
        const auto& camp = m_Outposts[i];
        file << "OUTPOST id=" << Outposts[i].id << " radius=" << camp.radius
             << " spawned=" << camp.spawned << " waves=" << camp.waves
             << " nextSpawn=" << camp.nextSpawn << " nextWave=" << camp.nextWave << '\n';
    }
    for (const auto& line : m_Log)
    {
        file << line << '\n';
    }
    for (const auto& post : m_CarrierPosts)
    {
        file << "PIGEON id=" << post.id << " from=" << Villages[post.source].id
             << " to=" << Villages[post.destination].id << " departed=" << post.departedAt
             << " arrives=" << post.arrivesAt << " reports=" << post.reports.size()
             << " events=" << post.events.size() << '\n';
    }
    file.close();
    return bool(file)
           && MoveFileExW(temporary.c_str(),
                          (m_LogDirectory / "latest.log").c_str(),
                          MOVEFILE_REPLACE_EXISTING);
}
