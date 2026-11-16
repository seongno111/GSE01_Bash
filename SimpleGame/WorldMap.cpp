#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"
#include "Outposts.h"
#include "Villages.h"

#include <algorithm>
#include <array>
#include <cmath>

using namespace GameRules;

struct MapView
{
    Point origin;
    WorldPoint start;
    WorldPoint end;
    float scale;

    Point Project(WorldPoint point) const
    {
        return {origin.x + float(point.x - start.x) * scale,
                origin.y + float(point.y - start.y) * scale};
    }

    WorldPoint Unproject(Point point) const
    {
        return {start.x + (point.x - origin.x) / scale, start.y + (point.y - origin.y) / scale};
    }

    bool Contains(WorldPoint point) const
    {
        return point.x >= start.x && point.x < end.x && point.y >= start.y && point.y < end.y;
    }

    void Fill(Renderer& r, WorldPoint first, WorldPoint last, Color color) const
    {
        first.x = (std::max)(first.x, start.x);
        first.y = (std::max)(first.y, start.y);
        last.x = (std::min)(last.x, end.x);
        last.y = (std::min)(last.y, end.y);
        if (first.x < last.x && first.y < last.y)
        {
            const Point p = Project(first);
            r.Rect(p.x,
                   p.y,
                   float(last.x - first.x) * scale,
                   float(last.y - first.y) * scale,
                   color);
        }
    }
};

namespace
{
    constexpr double OverviewWidth = 400;
    constexpr double OverviewHeight = 272;
    constexpr double MinimapWidth = 40;
    constexpr double MinimapHeight = 16;
    constexpr float MinimapScale = 4;
    constexpr int MinimumOverviewSize = 120;
    constexpr double ArrivalDistance = .06;
    const Color RoadColor(.44f, .41f, .30f);
    const Color CampColor(.91f, .49f, .38f);
    const Color PlayerColor(.98f, .98f, .94f);
    const std::array<Color, size_t(Terrain::Biome::Count)> BiomeColors = {Color(.26f, .34f, .25f),
                                                                          Color(.20f, .30f, .30f),
                                                                          Color(.16f, .28f, .22f),
                                                                          Color(.33f, .32f, .32f),
                                                                          Color(.36f, .27f, .25f),
                                                                          Color(.43f, .36f, .28f)};

    MapView OverviewView(int width, int height, WorldPoint center)
    {
        const float scale = (std::min)({float(width * .9 / OverviewWidth),
                                        float(height * .76 / OverviewHeight),
                                        2.5f});
        return {{(float(width) - float(OverviewWidth) * scale) * .5f,
                 (float(height) - float(OverviewHeight) * scale) * .5f},
                {center.x - OverviewWidth / 2, center.y - OverviewHeight / 2},
                {center.x + OverviewWidth / 2, center.y + OverviewHeight / 2},
                scale};
    }

    void DrawMarker(Renderer& r, const MapView& view, WorldPoint at, float size, Color color)
    {
        if (!view.Contains(at))
        {
            return;
        }
        Point p = view.Project(at);
        const Point end = view.Project(view.end);
        p.x = std::clamp(p.x, view.origin.x + size, end.x - size);
        p.y = std::clamp(p.y, view.origin.y + size, end.y - size);
        r.Triangle({p.x, p.y - size}, {p.x - size, p.y}, {p.x + size, p.y}, color);
        r.Triangle({p.x, p.y + size}, {p.x + size, p.y}, {p.x - size, p.y}, color);
    }

    void DrawLandmark(Renderer& r,
                      const MapView& view,
                      WorldPoint at,
                      const char* name,
                      Color color,
                      bool overview)
    {
        DrawMarker(r, view, at, overview ? 4.0f : 2.5f, color);
        if (overview && view.Contains(at))
        {
            const float size = (std::min)(1.15f, view.scale * .75f);
            const float width = r.TextWidth(name, size);
            const Point p = view.Project(at);
            const Point end = view.Project(view.end);
            const float x = std::clamp(p.x - width * .5f, view.origin.x + 4, end.x - width - 4);
            const float y = (std::min)(p.y + 8, end.y - 19 * size);
            r.Rect(x - 3, y - 1, width + 6, 17 * size + 2, Color(.04f, .05f, .05f, .89f));
            r.Text(x, y, name, color, size);
        }
    }
} // namespace

void Prototype::OpenWorldMap()
{
    const auto player = m_Player.WorldPosition();
    // Keep the settled region in a consistent frame while the player is nearby.
    m_MapCenter = {
        std::clamp(12.0, player.x - OverviewWidth / 2 + 40, player.x + OverviewWidth / 2 - 40),
        std::clamp(VillageSpacing / 2,
                   player.y - OverviewHeight / 2 + 36,
                   player.y + OverviewHeight / 2 - 36)};
    m_MapClickBlocked = false;
    m_WorldMapOpen = true;
}

bool Prototype::CloseWorldMap()
{
    const bool wasOpen = m_WorldMapOpen;
    m_WorldMapOpen = false;
    return wasOpen;
}

void Prototype::Click(float x, float y)
{
    if (!m_WorldMapOpen)
    {
        WeaponShopClick(x, y);
        return;
    }
    if (m_Width < MinimumOverviewSize || m_Height < MinimumOverviewSize)
    {
        return;
    }

    const auto view = OverviewView(m_Width, m_Height, m_MapCenter);
    const auto clicked = view.Unproject({x, y});
    if (!view.Contains(clicked))
    {
        return;
    }

    auto destination = clicked;
    if (!CanOccupy(clicked.x, clicked.y, .2))
    {
        // Landmark markers can sit on buildings or other solid objects.
        double closest = 2.5;
        for (int dy = -5; dy <= 5; ++dy)
        {
            for (int dx = -5; dx <= 5; ++dx)
            {
                const double distance = Distance(dx, dy) * .5;
                const WorldPoint candidate{clicked.x + dx * .5, clicked.y + dy * .5};
                if (distance < closest && CanOccupy(candidate.x, candidate.y, .2))
                {
                    closest = distance;
                    destination = candidate;
                }
            }
        }
        if (closest == 2.5)
        {
            m_MapClickBlocked = true;
            return;
        }
    }

    CancelMapTravel();
    m_MapDestination = destination;
    CloseWorldMap();
}

void Prototype::CancelMapTravel()
{
    if (!m_MapDestination)
    {
        return;
    }
    CancelRoute(m_Player.Id());
    m_MapDestination.reset();
    m_MapRoute.clear();
    m_MapRouteIndex = 0;
    m_MapTravelStalledTime = 0;
    m_MapTravelRetried = false;
}

void Prototype::UpdateMapTravel(float dt)
{
    const auto destination = *m_MapDestination;
    const double distance =
        Distance(destination.x - m_Player.WorldX(), destination.y - m_Player.WorldY());
    if (distance <= ArrivalDistance)
    {
        CancelMapTravel();
        Notice("목적지에 도착했습니다.");
        return;
    }
    if (m_MapRouteIndex >= m_MapRoute.size())
    {
        const auto result = RequestRoute(m_Player, destination, m_MapRoute);
        if (result == RouteStatus::Pending)
        {
            return;
        }
        if (result == RouteStatus::Unreachable
            || Distance(destination.x - m_MapRoute.back().x, destination.y - m_MapRoute.back().y)
                   >= distance)
        {
            CancelMapTravel();
            Notice("목적지로 이동할 수 없습니다.");
            return;
        }
        m_MapRouteIndex = 0;
    }

    double remaining = PlayerWalkSpeed * dt;
    while (remaining > 0 && m_MapRouteIndex < m_MapRoute.size())
    {
        const auto point = m_MapRoute[m_MapRouteIndex];
        const double dx = point.x - m_Player.WorldX();
        const double dy = point.y - m_Player.WorldY();
        const double length = Distance(dx, dy);
        if (length < .025)
        {
            ++m_MapRouteIndex;
            continue;
        }
        const double step = (std::min)({remaining, length, PlayerMoveStep});
        MoveActor(m_Player, dx / length * step, dy / length * step, PlayerRadius);
        remaining -= step;
        const double left = Distance(point.x - m_Player.WorldX(), point.y - m_Player.WorldY());
        m_MapTravelStalledTime =
            length - left < step * .1 ? m_MapTravelStalledTime + step / PlayerWalkSpeed : 0;
    }

    if (m_MapTravelStalledTime > .75)
    {
        // A collider can change when a streamed chunk becomes active.
        if (m_MapTravelRetried)
        {
            CancelMapTravel();
            Notice("장애물 때문에 자동 이동을 멈췄습니다.");
            return;
        }
        m_MapRoute.clear();
        m_MapRouteIndex = 0;
        m_MapTravelStalledTime = 0;
        m_MapTravelRetried = true;
    }
}

bool Prototype::MinimapBesideStatus() const
{
    return m_Width >= 840;
}

void Prototype::DrawMinimap(Renderer& r)
{
    const float width = float(MinimapWidth) * MinimapScale;
    const float height = float(MinimapHeight) * MinimapScale;
    const float x = float(m_Width) - width - 24;
    const float y = MinimapBesideStatus() ? 33.0f : 136.0f;
    const auto player = m_Player.WorldPosition();
    const MapView view{{x, y},
                       {player.x - MinimapWidth / 2, player.y - MinimapHeight / 2},
                       {player.x + MinimapWidth / 2, player.y + MinimapHeight / 2},
                       MinimapScale};
    const WorldPoint tileCenter{std::round(player.x), std::round(player.y)};
    if (!m_MinimapTileCenter || tileCenter.x != m_MinimapTileCenter->x
        || tileCenter.y != m_MinimapTileCenter->y)
    {
        m_MinimapTileCenter = tileCenter;
        m_MinimapProps.clear();
        const auto centerX = std::int64_t(tileCenter.x);
        const auto centerY = std::int64_t(tileCenter.y);
        for (auto ty = centerY - int(MinimapHeight / 2); ty <= centerY + int(MinimapHeight / 2);
             ++ty)
        {
            for (auto tx = centerX - int(MinimapWidth / 2); tx <= centerX + int(MinimapWidth / 2);
                 ++tx)
            {
                if (MakeTile(tx, ty).prop)
                {
                    m_MinimapProps.push_back({double(tx), double(ty)});
                }
            }
        }
    }

    r.Rect(x - 4, y - 22, width + 8, height + 26, Color(.08f, .10f, .11f, .98f));
    r.Text(x + 3, y - 18, "미니맵 [M]", Gold, 1.0f);
    if (m_MapDestination)
    {
        r.Text(x + 86, y - 18, "자동 이동", ExperienceColor, .9f);
    }
    DrawMap(r, view, false);
}

void Prototype::DrawWorldMap(Renderer& r) const
{
    if (!m_WorldMapOpen || m_Width < MinimumOverviewSize || m_Height < MinimumOverviewSize)
    {
        return;
    }

    const auto view = OverviewView(m_Width, m_Height, m_MapCenter);
    const Point end = view.Project(view.end);
    const float width = end.x - view.origin.x;
    const float height = end.y - view.origin.y;
    const float margin = float(m_Height) * .12f;
    const float ui = (std::min)({margin / 54, width / 530, 1.0f});
    r.Rect(0, 0, float(m_Width), float(m_Height), Color(.01f, .02f, .025f, .88f));
    r.Rect(view.origin.x - 5, view.origin.y - 5, width + 10, height + 10, Gold.Shade(.38f));
    r.Text(view.origin.x, view.origin.y - 39 * ui, "지역 지도", Gold, 2.0f * ui);
    const std::string controls = "[M / ESC] 닫기";
    r.Text(end.x - r.TextWidth(controls, 1.2f * ui),
           view.origin.y - 29 * ui,
           controls,
           Paper,
           1.2f * ui);

    DrawMap(r, view, true);
    const char* instruction = "클릭하여 목적지 지정 · WASD로 자동 이동 취소";
    if (m_MapClickBlocked)
    {
        instruction = "선택한 곳은 막혀 있습니다. 다른 지점을 선택하세요.";
    }
    r.Text(view.origin.x, end.y + 11 * ui, instruction, Paper, 1.25f * ui);
    const std::string playerLegend = "◆ 현재 위치";
    r.Text(view.origin.x, end.y + 34 * ui, playerLegend, PlayerColor, 1.0f * ui);
    r.Text(view.origin.x + (r.TextWidth(playerLegend, 1.0f) + 16) * ui,
           end.y + 34 * ui,
           "◆ 목적지",
           ExperienceColor,
           1.0f * ui);
}

void Prototype::DrawMap(Renderer& r, const MapView& view, bool overview) const
{
    constexpr int RegionSize = ChunkSize * 8;
    const auto firstX = std::int64_t(std::floor(view.start.x / RegionSize));
    const auto firstY = std::int64_t(std::floor(view.start.y / RegionSize));
    for (auto ry = firstY; double(ry * RegionSize) < view.end.y; ++ry)
    {
        for (auto rx = firstX; double(rx * RegionSize) < view.end.x; ++rx)
        {
            const auto biome = Terrain::RegionBiome(rx, ry, m_WorldSeed);
            view.Fill(r,
                      {double(rx * RegionSize), double(ry * RegionSize)},
                      {double((rx + 1) * RegionSize), double((ry + 1) * RegionSize)},
                      BiomeColors[size_t(biome)]);
        }
    }

    for (double x = NearestShrine(view.start.x); x < view.end.x + 1.5; x += ShrineSpacing)
    {
        view.Fill(r, {x - 1.5, view.start.y}, {x + 1.5, view.end.y}, RoadColor);
    }
    for (double y = NearestShrine(view.start.y); y < view.end.y + 1.5; y += ShrineSpacing)
    {
        view.Fill(r, {view.start.x, y - 1.5}, {view.end.x, y + 1.5}, RoadColor);
    }
    for (const auto& village : Villages)
    {
        const auto p = village.center;
        view.Fill(r,
                  {p.x - VillageHalfExtent, p.y - VillageHalfExtent},
                  {p.x + VillageHalfExtent, p.y + VillageHalfExtent},
                  Color(.48f, .40f, .28f, .55f));
        DrawLandmark(r, view, p, village.name, Gold, overview);
    }
    for (const auto& camp : Outposts)
    {
        const auto p = camp.center;
        view.Fill(r,
                  {p.x - OutpostHalfExtent, p.y - OutpostHalfExtent},
                  {p.x + OutpostHalfExtent, p.y + OutpostHalfExtent},
                  Color(.47f, .24f, .21f, .66f));
        DrawLandmark(r, view, p, camp.name, CampColor, overview);
    }
    if (!overview)
    {
        for (const auto prop : m_MinimapProps)
        {
            view.Fill(r, {prop.x - .35, prop.y - .35}, {prop.x + .35, prop.y + .35}, Ink);
        }
    }
    if (m_MapDestination)
    {
        DrawMarker(r, view, *m_MapDestination, overview ? 7.0f : 4.0f, ExperienceColor);
    }
    DrawMarker(r, view, m_Player.WorldPosition(), overview ? 6.0f : 4.0f, PlayerColor);
}
