#pragma once
#include "Actor.h"
#include <array>
#include <deque>
#include <functional>
#include <map>
#include <queue>
#include <vector>

// One A* search is resumed across frames. Requests retain IDs and value snapshots only.
struct NavigationSearch
{
    static constexpr int Width = 33;
    static constexpr int Count = Width * Width;
    static constexpr int Start = (Width / 2) * Width + Width / 2;
    using Entry = std::pair<double, int>;

    ActorId actor = InvalidActor;
    WorldPoint from;
    WorldPoint target;
    int left = 0;
    int top = 0;
    int best = Start;
    int lineIndex = 1;
    int lineSteps = 1;
    bool searching = false;
    bool goalReached = false;
    std::array<int, Count> previous{};
    std::array<int, Count> cost{};
    std::array<signed char, Count> passable{};
    std::array<bool, Count> closed{};
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;

    WorldPoint Point(int cell) const;
    double Heuristic(int cell) const;
};

struct NavigationRequest
{
    WorldPoint target;
    WorldPoint origin;
    double touchedAt = 0;
    bool complete = false;
    bool direct = false;
    std::vector<WorldPoint> path;
};

struct NavigationState
{
    static constexpr size_t Capacity = 512;
    std::map<ActorId, NavigationRequest> requests;
    std::deque<ActorId> pending;
    NavigationSearch search;
};
