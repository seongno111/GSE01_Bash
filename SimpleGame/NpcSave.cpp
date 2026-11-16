#include "stdafx.h"
#include "NpcWorld.h"
#include "Prototype.h"
#include <algorithm>

NpcWorld::SavedProgress NpcWorld::CaptureProgress() const
{
    SavedProgress state;
    state.Time = m_Time;
    state.Accumulator = m_Accumulator;
    state.Economies = m_Economies;
    state.Societies = m_Societies;
    state.Outposts = m_Outposts;
    state.Enemies = m_Enemies;
    state.CarrierPosts = m_CarrierPosts;
    state.NextCarrierPost = m_NextCarrierPost;
    state.NextWorldSimulation = m_NextWorldSimulation;
    state.NextMigrationReview = m_NextMigrationReview;
    state.RumorCursor = m_RumorCursor;
    state.Events = m_Events;
    state.WorldEvents = m_WorldEvents;
    state.EventCounts = m_EventCounts;
    state.Conduct = m_Conduct;
    state.UnlawfulConflicts = m_UnlawfulConflicts;
    state.NextWorldEvent = m_NextWorldEvent;
    state.NextRumor = m_NextRumor;
    state.NextExecution = m_NextExecution;
    state.PK = m_PK;
    return state;
}

void NpcWorld::RestoreProgress(SavedProgress state, const std::map<ActorId, ActorId>& ids)
{
    const auto remap = [&ids](ActorId old)
    {
        const auto found = ids.find(old);
        return found == ids.end() ? InvalidActor : found->second;
    };
    for (auto& id : state.Enemies)
    {
        id = remap(id);
    }
    for (auto& camp : state.Outposts)
    {
        for (auto& id : camp.members)
        {
            id = remap(id);
        }
    }
    const auto eventIds = [&remap](auto& event)
    {
        event.actor = remap(event.actor);
        event.subject = remap(event.subject);
        for (auto& id : event.knowers)
        {
            id = remap(id);
        }
        event.knowers.erase(std::remove(event.knowers.begin(), event.knowers.end(), InvalidActor),
                            event.knowers.end());
    };
    for (auto& event : state.WorldEvents)
    {
        eventIds(event);
    }
    for (auto& post : state.CarrierPosts)
    {
        auto& bird = m_Game.Scene().Spawn<CarrierBirdActor>(m_Game.m_GameplayRoot, m_Game, *this);
        post.bird = bird.Id();
        const auto from = Villages[post.source].center;
        const auto to = Villages[post.destination].center;
        const double t =
            std::clamp((state.Time - post.departedAt) / (post.arrivesAt - post.departedAt),
                       0.0,
                       1.0);
        bird.SetWorldPosition(from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t);
        for (auto& event : post.events)
        {
            eventIds(event);
        }
    }
    decltype(m_Conduct) conduct;
    for (const auto& entry : state.Conduct)
    {
        if (const auto id = remap(entry.first))
        {
            conduct[id] = entry.second;
        }
    }
    state.Conduct = std::move(conduct);
    decltype(m_UnlawfulConflicts) conflicts;
    for (const auto& pair : state.UnlawfulConflicts)
    {
        const auto a = remap(pair.first);
        const auto b = remap(pair.second);
        if (a && b)
        {
            conflicts.emplace(a, b); // Direction is attacker -> victim, not an unordered pair.
        }
    }
    state.UnlawfulConflicts = std::move(conflicts);
    m_Time = std::move(state.Time);
    m_Accumulator = std::move(state.Accumulator);
    m_Economies = std::move(state.Economies);
    m_Societies = std::move(state.Societies);
    m_Outposts = std::move(state.Outposts);
    m_Enemies = std::move(state.Enemies);
    m_CarrierPosts = std::move(state.CarrierPosts);
    m_NextCarrierPost = std::move(state.NextCarrierPost);
    m_NextWorldSimulation = std::move(state.NextWorldSimulation);
    m_NextMigrationReview = std::move(state.NextMigrationReview);
    m_RumorCursor = std::move(state.RumorCursor);
    m_Events = std::move(state.Events);
    m_WorldEvents = std::move(state.WorldEvents);
    m_EventCounts = std::move(state.EventCounts);
    m_Conduct = std::move(state.Conduct);
    m_UnlawfulConflicts = std::move(state.UnlawfulConflicts);
    m_NextWorldEvent = std::move(state.NextWorldEvent);
    m_NextRumor = std::move(state.NextRumor);
    m_NextExecution = std::move(state.NextExecution);
    m_PK = std::move(state.PK);
    m_Reservations.clear();
    m_Requests.clear();
    m_Selected = 0;
    m_NextExport = m_Time + 10;
    RebuildNpcSpatialIndex();
}
