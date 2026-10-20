#pragma once
#include "Actor.h"
#include "GameRules.h"
#include <array>

struct VillageDefinition
{
    const char* id;
    const char* name;
    const char* culture;
    WorldPoint center;

    bool Contains(WorldPoint point) const
    {
        return std::abs(point.x - center.x) <= GameRules::VillageHalfExtent
               && std::abs(point.y - center.y) <= GameRules::VillageHalfExtent;
    }
};

// Adjacent gates are 111 units apart: 30 seconds at the current walking speed.
inline constexpr double VillageSpacing = GameRules::ShrineSpacing;
inline constexpr std::array<VillageDefinition, 6> Villages = {
    {{"cathedral", "성당 마을", "자비와 상호부조", {0, 0}},
     {"dawn", "새벽 수도촌", "신앙과 정화", {-VillageSpacing, 0}},
     {"iron", "철벽 주둔촌", "규율과 무력", {0, VillageSpacing}},
     {"market", "황금 장터", "교역과 부의 축적", {VillageSpacing, VillageSpacing}},
     {"seekers", "잿빛 탐구촌", "탐험과 깨달음", {-VillageSpacing, VillageSpacing}},
     {"outlaws", "검은 이빨 소굴", "약탈과 힘의 숭배", {VillageSpacing, 0}}}};

inline WorldPoint VillageHome(size_t village, size_t resident)
{
    const auto center = Villages[village].center;
    return {center.x - 14 + double(resident % 8) * 4,
            center.y - 12 + double((resident / 8) % 7) * 4};
}

inline int VillageAt(WorldPoint point)
{
    for (size_t i = 0; i < Villages.size(); ++i)
    {
        if (Villages[i].Contains(point))
        {
            return static_cast<int>(i);
        }
    }

    return -1;
}

inline WorldPoint VillageRoadWaypoint(WorldPoint from, WorldPoint target)
{
    const int destinationVillage = VillageAt(target);
    if (GameRules::Length(target.x - from.x, target.y - from.y) <= GameRules::VillageHalfExtent * 2
        || destinationVillage < 0)
    {
        return target;
    }
    const auto destination = Villages[size_t(destinationVillage)].center;
    const int origin = VillageAt(from);
    if (origin >= 0 && origin != destinationVillage)
    {
        const auto center = Villages[size_t(origin)].center;
        const double dx = destination.x - center.x;
        const double dy = destination.y - center.y;
        if (std::abs(dx) >= std::abs(dy))
        {
            return {center.x + (dx < 0 ? -1 : 1) * (GameRules::VillageHalfExtent + 2), center.y};
        }
        return {center.x, center.y + (dy < 0 ? -1 : 1) * (GameRules::VillageHalfExtent + 2)};
    }
    const double roadX = GameRules::NearestShrine(from.x);
    const double roadY = GameRules::NearestShrine(from.y);
    if (std::abs(from.y - roadY) <= std::abs(from.x - roadX))
    {
        return std::abs(from.x - destination.x) > 1.5 ? WorldPoint{destination.x, roadY}
                                                      : destination;
    }
    return std::abs(from.y - destination.y) > 1.5 ? WorldPoint{roadX, destination.y} : destination;
}
