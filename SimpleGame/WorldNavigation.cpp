#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"
#include "Villages.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <queue>
#include <functional>

bool Prototype::FindRoute(WorldPoint from,
                          WorldPoint target,
                          std::vector<WorldPoint>& path,
                          std::uint64_t& visited) const
{
    path.clear();
    target = VillageRoadWaypoint(from, target);
    const auto walkable = [this](double x, double y)
    {
        return !Blocked(x - .2, y) && !Blocked(x + .2, y) && !Blocked(x, y - .2)
               && !Blocked(x, y + .2);
    };
    const auto clear = [&walkable](WorldPoint a, WorldPoint b)
    {
        const int count =
            (std::max)(1, int(std::ceil(GameRules::Length(a.x - b.x, a.y - b.y) / .2)));
        for (int i = 1; i <= count; ++i)
        {
            const double t = double(i) / count;
            if (!walkable(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t))
            {
                return false;
            }
        }
        return true;
    };
    const double distance = GameRules::Length(target.x - from.x, target.y - from.y);
    if (distance > 12)
    {
        target = {from.x + (target.x - from.x) * 12 / distance,
                  from.y + (target.y - from.y) * 12 / distance};
    }
    if (clear(from, target))
    {
        path.push_back(target);
        return true;
    }

    // Recenter a bounded search around the traveller, never around its home village.
    // Long trips consume cached route segments instead of searching the whole world.
    constexpr int Width = 33;
    constexpr int Count = Width * Width;
    const int left = int(std::round(from.x)) - Width / 2;
    const int top = int(std::round(from.y)) - Width / 2;
    const auto point = [left, top, Width](int cell)
    {
        return WorldPoint{double(left + cell % Width), double(top + cell / Width)};
    };
    const auto heuristic = [&point, target](int cell)
    {
        const auto p = point(cell);
        return GameRules::Length(p.x - target.x, p.y - target.y);
    };
    std::array<int, Count> previous;
    std::array<int, Count> cost;
    std::array<signed char, Count> passable{};
    previous.fill(-1);
    cost.fill(100000);
    using Entry = std::pair<double, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pending;
    constexpr int start = (Width / 2) * Width + Width / 2;
    previous[start] = start;
    cost[start] = 0;
    pending.push({heuristic(start), start});
    int best = start;
    const int offsets[] = {-1, 1, -Width, Width};
    std::array<bool, Count> closed{};
    while (!pending.empty())
    {
        const int current = pending.top().second;
        pending.pop();
        if (closed[current])
        {
            continue;
        }
        closed[current] = true;
        ++visited;
        const auto here = current == start ? from : point(current);
        if (heuristic(current) < heuristic(best))
        {
            best = current;
        }
        if (GameRules::Length(here.x - target.x, here.y - target.y) < .85 && clear(here, target))
        {
            best = current;
            break;
        }
        for (const int offset : offsets)
        {
            const int next = current + offset;
            if (next < 0 || next >= Count || closed[next]
                || (std::abs(offset) == 1 && current / Width != next / Width))
            {
                continue;
            }
            const auto p = point(next);
            if (passable[next] == 0)
            {
                passable[next] = walkable(p.x, p.y) ? 1 : -1;
            }
            if (passable[next] < 0 || cost[next] <= cost[current] + 1 || !clear(here, p))
            {
                continue;
            }
            cost[next] = cost[current] + 1;
            previous[next] = current;
            pending.push({cost[next] + heuristic(next), next});
        }
    }
    if (best == start)
    {
        return false;
    }
    for (int at = best; at != start; at = previous[at])
    {
        path.push_back(point(at));
    }
    std::reverse(path.begin(), path.end());
    if (clear(point(best), target))
    {
        path.push_back(target);
    }
    return !path.empty();
}
