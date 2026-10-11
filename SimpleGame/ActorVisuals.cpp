#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"

#include <cmath>

using namespace GameRules;

void Prototype::Block(Renderer& r, Point p, float width, float depth, float height, Color c)
{
    // Store exact shape/color values, never world coordinates or camera zoom.
    const float parameters[] = {width, depth, height, c.r, c.g, c.b, c.a, c.intensity};
    std::string key = "block/";
    key.append(reinterpret_cast<const char*>(parameters), sizeof(parameters));
    r.DrawCachedMesh(key,
                     p,
                     m_Zoom,
                     [&]()
                     {
                         const float w = width * m_Zoom;
                         const float d = depth * m_Zoom;
                         const float h = height * m_Zoom;
                         const Point top{p.x, p.y - d - h};
                         const Point left{p.x - w, p.y - h};
                         const Point right{p.x + w, p.y - h};
                         const Point bottom{p.x, p.y + d - h};
                         r.Quad(left, bottom, {p.x, p.y + d}, {p.x - w, p.y}, c.Shade(0.63f));
                         r.Quad(bottom, right, {p.x + w, p.y}, {p.x, p.y + d}, c.Shade(0.43f));
                         r.Quad(top, right, bottom, left, c);
                         r.Line(left, bottom, 1 * m_Zoom, c.Shade(1.15f));
                     });
}

void Prototype::Tree(Renderer& r, Point p, unsigned seed)
{
    r.DrawCachedMesh(
        "tree/" + std::to_string(seed % 40),
        p,
        m_Zoom,
        [&]()
        {
            const float z = m_Zoom;
            const float h = (65 + seed % 40) * z;
            const Color bark(0.15f, 0.19f, 0.18f);
            r.Ellipse({p.x + 16 * z, p.y + 6 * z},
                      33 * z,
                      9 * z,
                      Color(0.01f, 0.02f, 0.02f, 0.38f));
            r.Triangle({p.x - 6 * z, p.y}, {p.x + 5 * z, p.y}, {p.x + 3 * z, p.y - h}, bark);
            r.Line({p.x, p.y - h * 0.45f}, {p.x - 23 * z, p.y - h * 0.72f}, 4 * z, bark);
            r.Line({p.x - 23 * z, p.y - h * 0.72f}, {p.x - 29 * z, p.y - h * 0.94f}, 2 * z, bark);
            r.Line({p.x - 20 * z, p.y - h * 0.69f}, {p.x - 40 * z, p.y - h * 0.73f}, 2 * z, bark);
            r.Line({p.x + 1 * z, p.y - h * 0.65f},
                   {p.x + 25 * z, p.y - h * 0.85f},
                   3 * z,
                   bark.Shade(0.8f));
            r.Line({p.x + 25 * z, p.y - h * 0.85f}, {p.x + 27 * z, p.y - h * 1.04f}, 2 * z, bark);
            r.Line({p.x + 2 * z, p.y - h * 0.9f}, {p.x - 8 * z, p.y - h * 1.12f}, 2 * z, bark);
            r.Line({p.x - 2 * z, p.y - 10 * z},
                   {p.x - 1 * z, p.y - h * 0.63f},
                   z,
                   Color(0.31f, 0.34f, 0.28f));
        });
}

void Prototype::Chapel(Renderer& r, Point p)
{
    r.DrawCachedMesh(
        "chapel",
        p,
        m_Zoom,
        [&]()
        {
            const float z = m_Zoom;
            r.Ellipse({p.x + 45 * z, p.y + 15 * z},
                      135 * z,
                      31 * z,
                      Color(0.01f, 0.02f, 0.025f, 0.45f));
            Block(r, p, 99, 45, 12, Color(0.28f, 0.30f, 0.28f));
            Block(r, {p.x, p.y - 10 * z}, 73, 32, 77, Color(0.32f, 0.35f, 0.33f));
            // Broad facade with an exposed, fractured gable.
            r.Quad({p.x - 53 * z, p.y - 9 * z},
                   {p.x + 38 * z, p.y + 7 * z},
                   {p.x + 38 * z, p.y - 93 * z},
                   {p.x - 53 * z, p.y - 108 * z},
                   Color(0.24f, 0.28f, 0.27f));
            r.Triangle({p.x - 53 * z, p.y - 108 * z},
                       {p.x - 8 * z, p.y - 164 * z},
                       {p.x + 38 * z, p.y - 93 * z},
                       Color(0.29f, 0.33f, 0.30f));
            r.Triangle({p.x - 8 * z, p.y - 164 * z},
                       {p.x + 66 * z, p.y - 185 * z},
                       {p.x + 38 * z, p.y - 93 * z},
                       Color(0.12f, 0.18f, 0.19f));
            for (int i = 0; i < 5; ++i)
            {
                r.Line({p.x - 49 * z, p.y - (18 + i * 16) * z},
                       {p.x + 35 * z, p.y - (3 + i * 16) * z},
                       z,
                       Color(0.14f, 0.19f, 0.19f));
            }
            // Pointed doorway, window and cross are deliberately readable silhouettes.
            r.Rect(p.x - 20 * z, p.y - 48 * z, 25 * z, 41 * z, Ink);
            r.Triangle({p.x - 20 * z, p.y - 48 * z},
                       {p.x - 7 * z, p.y - 68 * z},
                       {p.x + 5 * z, p.y - 48 * z},
                       Ink);
            // Only the lantern surface emits. Its halo comes from post-process bloom.
            r.Rect(p.x - 13 * z, p.y - 37 * z, 12 * z, 18 * z, Ink);
            r.Rect(p.x - 11 * z,
                   p.y - 35 * z,
                   8 * z,
                   13 * z,
                   Color(0.98f, 0.63f, 0.22f).Radiance(4));
            r.Line({p.x - 7 * z, p.y - 37 * z}, {p.x - 7 * z, p.y - 20 * z}, z, Ink);
            r.Ellipse({p.x - 8 * z, p.y - 102 * z}, 12 * z, 18 * z, Ink);
            r.Ellipse({p.x - 8 * z, p.y - 102 * z},
                      7 * z,
                      13 * z,
                      Color(0.60f, 0.38f, 0.15f).Radiance(4));
            r.Line({p.x - 8 * z, p.y - 118 * z}, {p.x - 8 * z, p.y - 86 * z}, 3 * z, Ink);
            r.Line({p.x - 20 * z, p.y - 103 * z}, {p.x + 3 * z, p.y - 101 * z}, 3 * z, Ink);
            Block(r, {p.x - 63 * z, p.y - 7 * z}, 15, 9, 132, Color(0.34f, 0.37f, 0.33f));
            Block(r, {p.x + 52 * z, p.y + 7 * z}, 15, 9, 108, Color(0.31f, 0.34f, 0.30f));
            r.Triangle({p.x - 80 * z, p.y - 139 * z},
                       {p.x - 63 * z, p.y - 184 * z},
                       {p.x - 46 * z, p.y - 139 * z},
                       Color(0.13f, 0.19f, 0.19f));
            r.Line({p.x - 63 * z, p.y - 209 * z},
                   {p.x - 63 * z, p.y - 181 * z},
                   3 * z,
                   Gold.Shade(0.65f));
            r.Line({p.x - 73 * z, p.y - 200 * z},
                   {p.x - 53 * z, p.y - 200 * z},
                   3 * z,
                   Gold.Shade(0.65f));
            Block(r, {p.x + 52 * z, p.y - 101 * z}, 13, 8, 11, Color(0.39f, 0.40f, 0.34f));
            r.Line({p.x + 17 * z, p.y - 86 * z}, {p.x + 10 * z, p.y - 68 * z}, 2 * z, Ink);
            r.Line({p.x + 10 * z, p.y - 68 * z}, {p.x + 19 * z, p.y - 57 * z}, 2 * z, Ink);
            Block(r, {p.x + 78 * z, p.y + 36 * z}, 17, 9, 9, Color(0.29f, 0.31f, 0.28f));
        });
}

void Prototype::Shrine(Renderer& r, Point p)
{
    const float z = m_Zoom;
    Block(r, p, 30, 15, 7, Color(0.43f, 0.40f, 0.31f));
    Block(r, {p.x, p.y - 7 * z}, 17, 8, 20, Color(0.37f, 0.33f, 0.25f));
    r.Ellipse({p.x, p.y - 29 * z}, 21 * z, 8 * z, Color(0.16f, 0.17f, 0.14f));
    const float flicker = std::sin(m_Time * 9) * 3;
    // Draw the HDR flame and embers, not an additional translucent light disk.
    r.Triangle({p.x - 13 * z, p.y - 29 * z},
               {p.x + (3 + flicker) * z, p.y - 73 * z},
               {p.x + 14 * z, p.y - 29 * z},
               Color(0.95f, 0.39f, 0.10f).Radiance(5));
    r.Triangle({p.x - 7 * z, p.y - 29 * z},
               {p.x - flicker * z, p.y - 58 * z},
               {p.x + 7 * z, p.y - 29 * z},
               Color(1, 0.80f, 0.34f).Radiance(8));
    for (int i = 0; i < 7; ++i)
    {
        const float age = std::fmod(m_Time * 0.35f + i * 0.143f, 1.0f);
        r.Rect(p.x + std::sin(i * 8.0f + age * 5) * 12 * z,
               p.y - (32 + age * 77) * z,
               2 * z,
               2 * z,
               Color(1, 0.69f, 0.23f, 1 - age).Radiance(4));
    }
}

void Prototype::DrawCharacter(Renderer& r, Point p, int corruption, bool hostile)
{
    const bool monster = corruption >= MutationThreshold;
    const float z = m_Zoom * (monster ? 1.22f : 1.0f);
    const float bob = !hostile && m_Player.walking ? std::sin(m_Time * 13) * 2 * z : 0;
    const Color cloak = hostile   ? Color(0.30f, 0.10f, 0.23f)
                        : monster ? Color(0.28f, 0.19f, 0.33f)
                                  : Color(0.28f, 0.34f, 0.35f);
    r.Ellipse({p.x, p.y + 2 * z}, 17 * z, 7 * z, Color(0.005f, 0.01f, 0.02f, 0.55f));
    if (!hostile)
    {
        const Color ring = monster ? Violet : Gold;
        for (int i = 0; i < 24; ++i)
        {
            const float a = i * 6.2831853f / 24;
            const float b = (i + 1) * 6.2831853f / 24;
            r.Line({p.x + 19 * z * std::cos(a), p.y + 9 * z * std::sin(a)},
                   {p.x + 19 * z * std::cos(b), p.y + 9 * z * std::sin(b)},
                   z,
                   ring);
        }
    }
    p.y += bob;
    const unsigned variant =
        (monster ? 1u : 0u) | (hostile ? 2u : 0u) | (!hostile && m_Player.attack > 0 ? 4u : 0u);
    r.DrawCachedMesh(
        "actor/" + std::to_string(variant),
        p,
        z,
        [&]()
        {
            r.Line({p.x - 5 * z, p.y - 12 * z}, {p.x - 6 * z, p.y}, 5 * z, Ink);
            r.Line({p.x + 5 * z, p.y - 12 * z}, {p.x + 7 * z, p.y}, 5 * z, Ink);
            r.Triangle({p.x, p.y - 39 * z},
                       {p.x - 16 * z, p.y - 7 * z},
                       {p.x + 15 * z, p.y - 7 * z},
                       cloak);
            r.Triangle({p.x, p.y - 39 * z},
                       {p.x + 3 * z, p.y - 7 * z},
                       {p.x + 15 * z, p.y - 7 * z},
                       cloak.Shade(0.58f));
            r.Ellipse({p.x, p.y - 39 * z}, 10 * z, 12 * z, cloak.Shade(1.35f));
            r.Quad({p.x - 6 * z, p.y - 42 * z},
                   {p.x + 6 * z, p.y - 42 * z},
                   {p.x + 4 * z, p.y - 34 * z},
                   {p.x - 4 * z, p.y - 34 * z},
                   Ink);
            r.Rect(p.x - 4 * z,
                   p.y - 40 * z,
                   3 * z,
                   z,
                   monster ? Color(1, 0.3f, 0.55f).Radiance(3) : Gold);
            r.Rect(p.x + 2 * z,
                   p.y - 40 * z,
                   3 * z,
                   z,
                   monster ? Color(1, 0.3f, 0.55f).Radiance(3) : Gold);
            if (monster)
            {
                r.Triangle({p.x - 8 * z, p.y - 43 * z},
                           {p.x - 18 * z, p.y - 61 * z},
                           {p.x - 6 * z, p.y - 50 * z},
                           Violet.Shade(0.65f));
                r.Triangle({p.x + 8 * z, p.y - 43 * z},
                           {p.x + 17 * z, p.y - 62 * z},
                           {p.x + 6 * z, p.y - 50 * z},
                           Violet.Shade(0.8f));
                r.Line({p.x + 12 * z, p.y - 25 * z}, {p.x + 23 * z, p.y - 9 * z}, 4 * z, cloak);
                r.Line({p.x + 23 * z, p.y - 9 * z}, {p.x + 20 * z, p.y + 1 * z}, 2 * z, Paper);
            }
            else
            {
                r.Line({p.x + 13 * z, p.y - 23 * z},
                       {p.x + 20 * z, p.y - 3 * z},
                       3 * z,
                       Color(0.54f, 0.60f, 0.59f));
                r.Line({p.x + 10 * z, p.y - 19 * z}, {p.x + 19 * z, p.y - 23 * z}, 3 * z, Gold);
            }
            if (!hostile)
            {
                r.Line({p.x - 12 * z, p.y - 20 * z},
                       {p.x - 20 * z, p.y - 37 * z},
                       3 * z,
                       Color(0.38f, 0.28f, 0.17f));
                r.Triangle({p.x - 24 * z, p.y - 38 * z},
                           {p.x - 19 * z, p.y - 54 * z},
                           {p.x - 15 * z, p.y - 38 * z},
                           Gold.Radiance(6));
                if (m_Player.attack > 0)
                {
                    r.Ellipse({p.x, p.y - 24 * z},
                              3 * z,
                              3 * z,
                              Color(1, 0.78f, 0.35f).Radiance(4));
                }
            }
        });
}

void Prototype::DrawEnemyMesh(Renderer& r, Point p, const EnemyActor& enemy)
{
    const float z = m_Zoom * (enemy.tier == 2 ? 1.45f : 0.92f);
    const Color body = enemy.tier == 0   ? Color(0.32f, 0.40f, 0.29f)
                       : enemy.tier == 1 ? Color(0.38f, 0.28f, 0.24f)
                                         : Color(0.36f, 0.19f, 0.22f);
    r.Ellipse(p, 19 * z, 8 * z, Color(0.015f, 0.02f, 0.02f, 0.6f));
    if (enemy.attackWindup > 0)
    {
        r.Ellipse(p, 39 * z, 19 * z, Color(0.95f, 0.18f, 0.09f, 0.25f));
    }
    if (enemy.burstWindup > 0)
    {
        r.Ellipse(p, 90 * z, 45 * z, Color(0.95f, 0.12f, 0.09f, 0.15f));
        r.Ellipse({p.x, p.y - 32 * z}, 7 * z, 7 * z, Color(1, 0.19f, 0.08f).Radiance(4));
    }
    r.DrawCachedMesh(
        "farm-enemy/" + std::to_string(enemy.tier),
        p,
        z,
        [&]()
        {
            r.Line({p.x - 6 * z, p.y}, {p.x - 8 * z, p.y - 17 * z}, 5 * z, body.Shade(0.6f));
            r.Line({p.x + 6 * z, p.y}, {p.x + 8 * z, p.y - 17 * z}, 5 * z, body.Shade(0.6f));
            r.Triangle({p.x - 17 * z, p.y - 9 * z},
                       {p.x + 16 * z, p.y - 9 * z},
                       {p.x, p.y - 41 * z},
                       body);
            r.Ellipse({p.x, p.y - 35 * z}, 10 * z, 12 * z, body.Shade(1.3f));
            r.Rect(p.x - 6 * z, p.y - 38 * z, 12 * z, 5 * z, Color(0.035f, 0.04f, 0.035f));
            r.Rect(p.x - 4 * z, p.y - 37 * z, 2 * z, 2 * z, Color(0.82f, 0.88f, 0.4f).Radiance(2));
            r.Rect(p.x + 3 * z, p.y - 37 * z, 2 * z, 2 * z, Color(0.82f, 0.88f, 0.4f).Radiance(2));
            r.Line({p.x + 12 * z, p.y - 23 * z}, {p.x + 23 * z, p.y - 8 * z}, 4 * z, body);
            if (enemy.tier == 2)
            {
                r.Line({p.x - 14 * z, p.y - 23 * z},
                       {p.x - 23 * z, p.y + 2 * z},
                       5 * z,
                       Color(0.50f, 0.48f, 0.36f));
                r.Triangle({p.x - 9 * z, p.y - 42 * z},
                           {p.x - 15 * z, p.y - 57 * z},
                           {p.x - 5 * z, p.y - 43 * z},
                           body);
                r.Triangle({p.x + 9 * z, p.y - 42 * z},
                           {p.x + 15 * z, p.y - 57 * z},
                           {p.x + 5 * z, p.y - 43 * z},
                           body);
            }
        });
}

void Prototype::DrawTile(Renderer& r, const WorldTileData& tile, Point p)
{

    const unsigned flags =
        (tile.road ? 1u : 0u) | (LevelOneCell(tile.x, tile.y) >= 0 ? 2u : 0u)
        | (tile.taint > 0.56f ? 4u : 0u) | (tile.taint > 0.61f ? 8u : 0u)
        | (m_Grid && (tile.x % ChunkSize == 0 || tile.y % ChunkSize == 0) ? 16u : 0u);
    const std::string key = "terrain/" + std::to_string(tile.variant) + "/" + std::to_string(flags);
    r.DrawCachedMesh(
        key,
        p,
        m_Zoom,
        [&]()
        {
            const float v = 0.86f + float(tile.variant % 100) * 0.002f;
            Color c = tile.road ? Color(0.27f, 0.285f, 0.25f) : Color(0.16f, 0.215f, 0.20f);
            if (LevelOneCell(tile.x, tile.y) >= 0)
            {
                c = tile.road ? Color(0.32f, 0.29f, 0.21f) : Color(0.23f, 0.24f, 0.17f);
            }
            if (!tile.road && tile.taint > 0.56f)
            {
                c = Color(0.20f, 0.155f, 0.22f);
            }
            const float w = 40.35f * m_Zoom;
            const float h = 20.25f * m_Zoom;
            r.Quad({p.x, p.y - h}, {p.x + w, p.y}, {p.x, p.y + h}, {p.x - w, p.y}, c.Shade(v));
            if (tile.road)
            {
                r.Line({p.x - w + 2 * m_Zoom, p.y},
                       {p.x, p.y + h - m_Zoom},
                       m_Zoom,
                       Color(0.10f, 0.14f, 0.14f, 0.5f));
                if (tile.variant % 3 == 0)
                {
                    r.Line({p.x - 9 * m_Zoom, p.y - 9 * m_Zoom},
                           {p.x + 5 * m_Zoom, p.y + 8 * m_Zoom},
                           m_Zoom,
                           c.Shade(0.6f));
                }
            }
            else
            {
                for (int i = 0; i < 3; ++i)
                {
                    const float ox = float((tile.variant >> (i * 5)) % 29) - 14;
                    const float oy = float((tile.variant >> (i * 4 + 2)) % 13) - 6;
                    r.Line({p.x + ox * m_Zoom, p.y + oy * m_Zoom},
                           {p.x + (ox + 4) * m_Zoom, p.y + (oy - 2) * m_Zoom},
                           m_Zoom,
                           c.Shade(1.25f));
                }
                if (tile.taint > 0.61f && tile.variant % 4 == 0)
                {
                    r.Ellipse(p, 24 * m_Zoom, 10 * m_Zoom, Color(0.12f, 0.065f, 0.16f, 0.8f));
                    r.Line({p.x - 16 * m_Zoom, p.y},
                           {p.x + 8 * m_Zoom, p.y - 5 * m_Zoom},
                           m_Zoom,
                           Color(0.46f, 0.26f, 0.43f, 0.6f));
                }
            }
            if (m_Grid && (tile.x % ChunkSize == 0 || tile.y % ChunkSize == 0))
            {
                r.Line({p.x, p.y - h}, {p.x + w, p.y}, m_Zoom, Color(0.5f, 0.78f, 0.7f, 0.6f));
                r.Line({p.x, p.y - h}, {p.x - w, p.y}, m_Zoom, Color(0.5f, 0.78f, 0.7f, 0.6f));
            }
        });
}

void Prototype::DrawProp(Renderer& r, Point p, int kind, unsigned seed)
{
    if (kind >= 7 && kind <= 9)
    {
        const float z = m_Zoom;
        r.DrawCachedMesh(
            "village/building/v1/" + std::to_string(kind),
            p,
            z,
            [&]()
            {
                if (kind == 9)
                {
                    Block(r, p, 16, 8, 4, Color(0.34f, 0.30f, 0.23f));
                    r.Line({p.x, p.y}, {p.x, p.y - 45 * z}, 5 * z, Gold.Shade(0.6f));
                    r.Line({p.x - 15 * z, p.y - 29 * z},
                           {p.x + 15 * z, p.y - 29 * z},
                           4 * z,
                           Paper);
                }
                else
                {
                    Block(r, p, 38, 23, kind == 7 ? 46.0f : 25.0f, Color(0.31f, 0.28f, 0.23f));
                    const float roof = kind == 7 ? 46.0f : 25.0f;
                    r.Triangle({p.x - 43 * z, p.y - roof * z},
                               {p.x + 43 * z, p.y - roof * z},
                               {p.x, p.y - (roof + 38) * z},
                               kind == 7 ? Color(0.20f, 0.24f, 0.23f) : Color(0.50f, 0.32f, 0.19f));
                    r.Rect(p.x - 8 * z, p.y - 28 * z, 16 * z, 30 * z, Ink);
                    r.Rect(p.x - 26 * z, p.y - 33 * z, 9 * z, 11 * z, Gold.Radiance(3));
                }
            });
        return;
    }

    if (kind == 1)
    {
        Tree(r, p, seed);
    }
    if (kind == 2)
    {
        Block(r, p, 24, 12, 37, Color(0.29f, 0.33f, 0.30f));
        Block(r, {p.x - 9 * m_Zoom, p.y - 35 * m_Zoom}, 12, 6, 14, Color(0.34f, 0.37f, 0.32f));
    }
    if (kind == 3)
    {
        Block(r, p, 12 + float(seed % 8), 9, 10 + float(seed % 11), Color(0.25f, 0.29f, 0.28f));
    }
    if (kind == 4)
    {
        Block(r, p, 10, 5, 7, Color(0.27f, 0.30f, 0.28f));
        r.Line({p.x, p.y - 5 * m_Zoom}, {p.x, p.y - 38 * m_Zoom}, 5 * m_Zoom, Muted.Shade(0.65f));
        r.Line({p.x - 9 * m_Zoom, p.y - 28 * m_Zoom},
               {p.x + 9 * m_Zoom, p.y - 28 * m_Zoom},
               4 * m_Zoom,
               Muted.Shade(0.65f));
    }
    if (kind == 5)
    {
        Chapel(r, p);
    }
    if (kind == 6)
    {
        Shrine(r, p);
    }
}
