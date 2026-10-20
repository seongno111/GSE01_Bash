#include "stdafx.h"
#include "NpcWorld.h"
#include "Prototype.h"
#include "GameRules.h"
#include <algorithm>
#include <cmath>
#include <utility>

using namespace GameRules;

CarrierBirdActor::CarrierBirdActor(Prototype& game, NpcWorld& world)
    : GameActor(game, "전서구", TickGroup::None, ActorLayer::World),
      m_World(world)
{
}

void CarrierBirdActor::Draw(Renderer& renderer)
{
    const float zoom = m_Game.Zoom();
    Point position = m_Game.Project(WorldX(), WorldY());
    position.y -= 35 * zoom;
    const int flap = int(m_World.Time() * 8) % 4;
    renderer.DrawCachedMesh(
        "courier/pigeon-v1/" + std::to_string(flap),
        position,
        zoom,
        [&]()
        {
            const float wings[] = {-6, -2, 4, -2};
            const float wing = wings[flap];
            renderer.Ellipse(position, 3 * zoom, 2 * zoom, Color(.78f, .77f, .69f), 8);
            renderer.Triangle(position,
                              {position.x - 10 * zoom, position.y + wing * zoom},
                              {position.x - 3 * zoom, position.y + 2 * zoom},
                              Color(.55f, .57f, .55f));
            renderer.Triangle(position,
                              {position.x + 10 * zoom, position.y + wing * zoom},
                              {position.x + 3 * zoom, position.y + 2 * zoom},
                              Color(.66f, .67f, .61f));
        });
}

void NpcWorld::RebuildNpcSpatialIndex()
{
    // Preserve bucket allocations across ticks; queries only examine nearby cells.
    for (auto& entry : m_NpcSpatialIndex)
    {
        entry.second.clear();
    }
    for (auto id : m_Actors)
    {
        const auto* npc = static_cast<const NpcActor*>(m_Game.Scene().Find(id));
        if (npc && npc->vitals.alive && npc->EnabledInHierarchy())
        {
            m_NpcSpatialIndex[{int(std::floor(npc->WorldX() / 8)),
                               int(std::floor(npc->WorldY() / 8))}]
                .push_back(id);
        }
    }
    for (auto it = m_NpcSpatialIndex.begin(); it != m_NpcSpatialIndex.end();)
    {
        if (it->second.empty())
        {
            it = m_NpcSpatialIndex.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

std::vector<ActorId> NpcWorld::NearbyNpcs(WorldPoint position, double radius) const
{
    if (m_NpcSpatialIndex.empty())
    {
        return m_Actors;
    }
    std::vector<ActorId> result;
    for (int y = int(std::floor((position.y - radius) / 8));
         y <= int(std::floor((position.y + radius) / 8));
         ++y)
    {
        for (int x = int(std::floor((position.x - radius) / 8));
             x <= int(std::floor((position.x + radius) / 8));
             ++x)
        {
            const auto found = m_NpcSpatialIndex.find({x, y});
            if (found != m_NpcSpatialIndex.end())
            {
                result.insert(result.end(), found->second.begin(), found->second.end());
            }
        }
    }
    return result;
}

double NpcWorld::JourneyBudget(WorldPoint from, WorldPoint to, float speed) const
{
    // Account for the road's horizontal/vertical legs, rest and combat interruptions.
    return (std::abs(from.x - to.x) + std::abs(from.y - to.y)) / (std::max)(0.5f, speed) * 1.8 + 60;
}

void NpcWorld::UpdateCommunication()
{
    for (auto it = m_CarrierPosts.begin(); it != m_CarrierPosts.end();)
    {
        auto* bird = m_Game.Scene().Find(it->bird);
        const auto from = Villages[it->source].center;
        const auto to = Villages[it->destination].center;
        const double progress =
            std::clamp((m_Time - it->departedAt) / (it->arrivesAt - it->departedAt), 0.0, 1.0);
        if (bird)
        {
            bird->SetWorldPosition(from.x + (to.x - from.x) * progress,
                                   from.y + (to.y - from.y) * progress);
        }
        if (m_Time < it->arrivesAt)
        {
            ++it;
            continue;
        }
        auto& receiver = m_Societies[it->destination];
        for (const auto& report : it->reports)
        {
            const auto known = receiver.known.find(report.first);
            if (known == receiver.known.end()
                || report.second.observedAt > known->second.observedAt)
            {
                receiver.known[report.first] = report.second;
            }
        }
        // The village noticeboard distributes received reports to actual residents
        // present here. Rumors never populate combat sight or target memory.
        for (auto id : m_Actors)
        {
            auto* npc = static_cast<NpcActor*>(m_Game.Scene().Find(id));
            if (!npc || !npc->vitals.alive || npc->profile.village != it->destination
                || VillageAt(npc->WorldPosition()) != int(it->destination))
            {
                continue;
            }
            for (const auto& report : receiver.known)
            {
                const auto known = npc->villageKnowledge.find(report.first);
                if (known == npc->villageKnowledge.end()
                    || report.second.observedAt > known->second.observedAt)
                {
                    npc->villageKnowledge[report.first] = report.second;
                }
            }
        }
        ++receiver.postsReceived;
        const WorldPoint noticeboard{to.x + 2, to.y + 1};
        RecordWorldEvent("pigeon_arrival",
                         std::string(Villages[it->source].name) + " → "
                             + Villages[it->destination].name + " 전서구 도착 #"
                             + std::to_string(it->id),
                         noticeboard);
        for (const auto& event : it->events)
        {
            RecordWorldEvent("pigeon_rumor",
                             "전서구 소식: " + event.text,
                             noticeboard,
                             nullptr,
                             nullptr,
                             event.id);
        }
        if (bird)
        {
            bird->Destroy();
        }
        it = m_CarrierPosts.erase(it);
    }

    for (size_t source = 0; source < Villages.size(); ++source)
    {
        auto& sender = m_Societies[source];
        if (m_Time < sender.nextPost)
        {
            continue;
        }
        sender.nextPost = m_Time + 24;
        bool inhabited = false;
        for (auto id : m_Actors)
        {
            const auto* npc = static_cast<const NpcActor*>(m_Game.Scene().Find(id));
            inhabited = inhabited
                        || (npc && npc->vitals.alive && npc->profile.village == source
                            && VillageAt(npc->WorldPosition()) == int(source));
        }
        if (!inhabited)
        {
            continue;
        }
        std::vector<size_t> neighbors;
        for (const auto& report : sender.known)
        {
            if (report.first != source)
            {
                neighbors.push_back(report.first);
            }
        }
        if (neighbors.empty())
        {
            continue;
        }
        CarrierPost post;
        post.id = m_NextCarrierPost++;
        post.source = source;
        post.destination = neighbors[sender.postsSent % neighbors.size()];
        post.departedAt = m_Time;
        const auto from = Villages[source].center;
        const auto to = Villages[post.destination].center;
        post.arrivesAt = m_Time + Length(to.x - from.x, to.y - from.y) / CarrierPigeonSpeed;
        post.reports = sender.known;
        for (auto event = m_WorldEvents.rbegin();
             event != m_WorldEvents.rend() && post.events.size() < 4;
             ++event)
        {
            if (event->time < m_Time - 90)
            {
                break;
            }
            if (event->type != "murder" && event->type != "raid" && event->type != "hunt"
                && event->type != "monster_wave" && event->type != "frenzy")
            {
                continue;
            }
            const bool known =
                std::any_of(event->knowers.begin(),
                            event->knowers.end(),
                            [&](ActorId id)
                            {
                                const auto* npc =
                                    dynamic_cast<const NpcActor*>(m_Game.Scene().Find(id));
                                return npc && npc->vitals.alive && npc->profile.village == source;
                            });
            if (known)
            {
                post.events.push_back(*event);
            }
        }
        auto& bird = m_Game.Scene().Spawn<CarrierBirdActor>(m_Game.m_GameplayRoot, m_Game, *this);
        bird.SetWorldPosition(from.x, from.y);
        post.bird = bird.Id();
        ++sender.postsSent;
        RecordWorldEvent("pigeon_departure",
                         std::string(Villages[source].name) + " → "
                             + Villages[post.destination].name + " 전서구 발송 #"
                             + std::to_string(post.id) + ", 비행 "
                             + std::to_string(int(std::ceil(post.arrivesAt - m_Time))) + "초",
                         from);
        m_CarrierPosts.push_back(std::move(post));
    }
}
