#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"
#include "Villages.h"
#include "NpcAI.h"
#include <algorithm>
#include <cmath>
#include <chrono>

namespace
{
    bool CanNavigate(const Actor* actor)
    {
        if (!actor || !actor->EnabledInHierarchy())
        {
            return false;
        }
        const auto* enemy = dynamic_cast<const EnemyActor*>(actor);
        const auto* npc = dynamic_cast<const NpcActor*>(actor);
        return (!enemy || enemy->alive) && (!npc || npc->vitals.alive);
    }
} // namespace

WorldPoint NavigationSearch::Point(int cell) const
{
    return cell == Start ? from
                         : WorldPoint{double(left + cell % Width), double(top + cell / Width)};
}

double NavigationSearch::Heuristic(int cell) const
{
    const auto p = Point(cell);
    return GameRules::Length(p.x - target.x, p.y - target.y);
}

bool Prototype::ClearMovement(WorldPoint from, WorldPoint to, double radius) const
{
    const double dx = to.x - from.x;
    const double dy = to.y - from.y;
    const int steps = (std::max)(1, int(std::ceil(GameRules::Length(dx, dy) / .2)));
    for (int i = 1; i <= steps; ++i)
    {
        const double t = double(i) / steps;
        if (!CanOccupy(from.x + dx * t, from.y + dy * t, radius))
        {
            return false;
        }
    }
    return true;
}

void Prototype::CancelRoute(ActorId actor)
{
    m_Navigation.requests.erase(actor);
    auto& pending = m_Navigation.pending;
    pending.erase(std::remove(pending.begin(), pending.end(), actor), pending.end());
    if (m_Navigation.search.actor == actor)
    {
        m_Navigation.search.actor = InvalidActor;
    }
}

Prototype::RouteStatus
Prototype::RequestRoute(const Actor& actor, WorldPoint target, std::vector<WorldPoint>& path)
{
    auto& requests = m_Navigation.requests;
    auto found = requests.find(actor.Id());
    if (found != requests.end())
    {
        auto& request = found->second;
        request.touchedAt = m_Time;
        if (!request.complete)
        {
            if (m_Navigation.search.actor != actor.Id())
            {
                request.target = target;
            }
            ++m_UpdateStats.pathDeferred;
            return RouteStatus::Pending;
        }

        // A cached path may have carried the actor onward while this request waited.
        // Solve from its new position rather than making it walk back to the old origin.
        if ((!request.direct
             && GameRules::Length(actor.WorldX() - request.origin.x,
                                  actor.WorldY() - request.origin.y)
                    > .00001)
            || GameRules::Length(target.x - request.target.x, target.y - request.target.y) > 1)
        {
            CancelRoute(actor.Id());
        }
        else
        {
            const bool reached = !request.path.empty();
            path = std::move(request.path);
            requests.erase(found);
            return reached ? RouteStatus::Found : RouteStatus::Unreachable;
        }
    }

    if (requests.size() < NavigationState::Capacity)
    {
        NavigationRequest request;
        request.target = target;
        request.touchedAt = m_Time;
        requests.emplace(actor.Id(), std::move(request));
        m_Navigation.pending.push_back(actor.Id());
    }
    ++m_UpdateStats.pathDeferred;
    return RouteStatus::Pending;
}

void Prototype::ProcessNavigation()
{
    using Clock = std::chrono::steady_clock;
    const auto started = Clock::now();
    const auto deadline = started + std::chrono::microseconds(1500);
    auto& navigation = m_Navigation;
    // Finished or abandoned requests occupy at most one slot per actor.
    for (auto it = navigation.requests.begin(); it != navigation.requests.end();)
    {
        const auto* actor = m_Scene.Find(it->first);
        if (!CanNavigate(actor) || m_Time - it->second.touchedAt > 5)
        {
            const auto id = it->first;
            ++it;
            CancelRoute(id);
        }
        else
        {
            ++it;
        }
    }

    // A hard operation count supplements the wall-clock budget on very fast machines.
    // Direct visibility checks and A* expansions both yield between operations.
    int operations = 0;
    constexpr double RouteRadius = .2;
    constexpr int MaxOperations = 192;
    while (operations < MaxOperations && Clock::now() < deadline)
    {
        auto& search = navigation.search;
        if (search.actor == InvalidActor)
        {
            if (navigation.pending.empty())
            {
                break;
            }

            const auto actorId = navigation.pending.front();
            navigation.pending.pop_front();
            auto request = navigation.requests.find(actorId);
            const auto* actor = m_Scene.Find(actorId);
            if (request == navigation.requests.end() || !CanNavigate(actor))
            {
                CancelRoute(actorId);
                continue;
            }

            search = {};
            search.actor = actorId;
            search.from = actor->WorldPosition();
            request->second.origin = search.from;
            search.target = VillageRoadWaypoint(search.from, request->second.target);
            const double distance =
                GameRules::Length(search.target.x - search.from.x, search.target.y - search.from.y);
            if (distance > 12)
            {
                search.target = {search.from.x + (search.target.x - search.from.x) * 12 / distance,
                                 search.from.y + (search.target.y - search.from.y) * 12 / distance};
            }
            search.lineSteps =
                (std::max)(1,
                           int(std::ceil(GameRules::Length(search.target.x - search.from.x,
                                                           search.target.y - search.from.y)
                                         / .2)));
            ++m_UpdateStats.pathCalls;
        }

        auto& request = navigation.requests.at(search.actor);
        ++operations;
        if (!search.searching)
        {
            const double t = double(search.lineIndex) / search.lineSteps;
            if (CanOccupy(search.from.x + (search.target.x - search.from.x) * t,
                          search.from.y + (search.target.y - search.from.y) * t,
                          RouteRadius))
            {
                ++search.lineIndex;
                if (search.lineIndex > search.lineSteps)
                {
                    request.path.push_back(search.target);
                    request.direct = true;
                    request.complete = true;
                    ++m_UpdateStats.pathCompleted;
                    search.actor = InvalidActor;
                }
                continue;
            }

            search.searching = true;
            search.left = int(std::round(search.from.x)) - NavigationSearch::Width / 2;
            search.top = int(std::round(search.from.y)) - NavigationSearch::Width / 2;
            search.previous.fill(-1);
            search.cost.fill(100000);
            search.previous[NavigationSearch::Start] = NavigationSearch::Start;
            search.cost[NavigationSearch::Start] = 0;
            search.open.push({search.Heuristic(NavigationSearch::Start), NavigationSearch::Start});
            continue;
        }

        if (!search.open.empty())
        {
            const int current = search.open.top().second;
            search.open.pop();
            if (search.closed[current])
            {
                continue;
            }
            search.closed[current] = true;
            ++m_UpdateStats.pathNodes;
            const auto here = search.Point(current);
            if (search.Heuristic(current) < search.Heuristic(search.best))
            {
                search.best = current;
            }
            if (GameRules::Length(here.x - search.target.x, here.y - search.target.y) < .85
                && ClearMovement(here, search.target, RouteRadius))
            {
                search.best = current;
                search.goalReached = true;
                search.open = {};
            }
            else
            {
                const int offsets[] = {-1, 1, -NavigationSearch::Width, NavigationSearch::Width};
                for (const int offset : offsets)
                {
                    const int next = current + offset;
                    if (next < 0 || next >= NavigationSearch::Count || search.closed[next]
                        || (std::abs(offset) == 1
                            && current / NavigationSearch::Width != next / NavigationSearch::Width))
                    {
                        continue;
                    }
                    const auto point = search.Point(next);
                    if (search.passable[next] == 0)
                    {
                        search.passable[next] = CanOccupy(point.x, point.y, RouteRadius) ? 1 : -1;
                    }
                    if (search.passable[next] < 0 || search.cost[next] <= search.cost[current] + 1
                        || !ClearMovement(here, point, RouteRadius))
                    {
                        continue;
                    }
                    search.cost[next] = search.cost[current] + 1;
                    search.previous[next] = current;
                    search.open.push({search.cost[next] + search.Heuristic(next), next});
                }
            }
        }

        if (search.open.empty())
        {
            if (search.best != NavigationSearch::Start)
            {
                for (int at = search.best; at != NavigationSearch::Start; at = search.previous[at])
                {
                    request.path.push_back(search.Point(at));
                }
                std::reverse(request.path.begin(), request.path.end());
                if (search.goalReached)
                {
                    request.path.push_back(search.target);
                }
            }
            request.complete = true;
            ++m_UpdateStats.pathCompleted;
            search.actor = InvalidActor;
        }
    }

    m_UpdateStats.pathPending =
        navigation.pending.size() + (navigation.search.actor != InvalidActor ? 1 : 0);
    m_UpdateStats.pathMs =
        std::chrono::duration<double, std::milli>(Clock::now() - started).count();
}
