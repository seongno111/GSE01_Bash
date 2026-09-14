#include "stdafx.h"
#include "Prototype.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <sstream>
#include <iomanip>

namespace
{
    constexpr int ChunkSize = 8;
    constexpr int ShrineSpacing = 32;
    constexpr int MutationThreshold = 100;
    const Color Ink(0.025f, 0.035f, 0.042f);
    const Color Gold(0.84f, 0.65f, 0.36f);
    const Color Paper(0.78f, 0.79f, 0.73f);
    const Color Muted(0.43f, 0.52f, 0.51f);
    const Color Violet(0.65f, 0.30f, 0.65f);

    std::uint64_t Hash(std::int64_t x, std::int64_t y, std::uint64_t seed)
    {
        std::uint64_t n = std::uint64_t(x) * 0x9E3779B185EBCA87ULL
                          ^ std::uint64_t(y) * 0xC2B2AE3D27D4EB4FULL ^ seed;
        n ^= n >> 30;
        n *= 0xBF58476D1CE4E5B9ULL;
        n ^= n >> 27;
        n *= 0x94D049BB133111EBULL;
        return n ^ (n >> 31);
    }

    float Noise(std::int64_t x, std::int64_t y, std::uint64_t seed)
    {
        return float(Hash(x, y, seed) & 65535) / 65535.0f;
    }

    float SmoothNoise(double x, double y, std::uint64_t seed)
    {
        const auto ix = static_cast<std::int64_t>(std::floor(x));
        const auto iy = static_cast<std::int64_t>(std::floor(y));
        float fx = float(x - ix);
        float fy = float(y - iy);
        fx = fx * fx * (3 - 2 * fx);
        fy = fy * fy * (3 - 2 * fy);
        const float a = Noise(ix, iy, seed) * (1 - fx) + Noise(ix + 1, iy, seed) * fx;
        const float b = Noise(ix, iy + 1, seed) * (1 - fx) + Noise(ix + 1, iy + 1, seed) * fx;
        return a * (1 - fy) + b * fy;
    }

    double Distance(double x, double y)
    {
        return std::sqrt(x * x + y * y);
    }

    std::int64_t ChunkOf(double p)
    {
        return static_cast<std::int64_t>(std::floor(p / ChunkSize));
    }

    double NearestShrine(double p)
    {
        return std::round(p / ShrineSpacing) * ShrineSpacing;
    }
} // namespace

Prototype::Tile Prototype::MakeTile(std::int64_t x, std::int64_t y) const
{
    const auto seed = Hash(x, y, m_WorldSeed);
    const double lx = double(x) - NearestShrine(double(x));
    const double ly = double(y) - NearestShrine(double(y));
    const bool plaza = std::abs(lx) <= 3 && std::abs(ly) <= 3;
    const bool road = lx == 0 || ly == 0 || plaza;
    float taint = SmoothNoise(double(x) / 7.0 + 17, double(y) / 7.0 - 29, m_WorldSeed);
    if (plaza)
    {
        taint *= 0.25f;
    }
    int prop = 0;
    // Keep the landmark and its approach clear, including the respawn point.
    if (!road && x % 4 != 0 && y % 4 != 0 && !(std::abs(lx) < 6 && std::abs(ly) < 6))
    {
        if (seed % 19 == 0)
        {
            prop = 1; // dead tree
        }
        else if (seed % 29 == 0)
        {
            prop = 2; // broken wall
        }
        else if (seed % 13 == 0)
        {
            prop = 3; // rock
        }
        else if (seed % 37 == 0)
        {
            prop = 4; // grave marker
        }
    }
    const int cell = LevelOneCell(x, y);
    if (cell >= 0 && !m_LevelWalkable.empty())
    {
        // Connected walkable cells are clear; unreachable pockets become solid terrain.
        prop = m_LevelWalkable[cell] ? 0 : 2;
        return {x,
                y,
                taint * 0.7f,
                static_cast<unsigned>(seed >> 32),
                prop,
                m_LevelWalkable[cell] && (y == 3 || x % 3 == 0)};
    }

    return {x, y, taint, static_cast<unsigned>(seed >> 32), prop, road};
}

void Prototype::Stream()
{
    const auto cx = ChunkOf(m_X);
    const auto cy = ChunkOf(m_Y);
    // Account for all four screen corners in the inverse isometric projection.
    m_Radius = int(std::ceil((m_Width / 160.0 + m_Height / 80.0 + 8) / m_Zoom / ChunkSize)) + 1;
    for (auto it = m_Chunks.begin(); it != m_Chunks.end();)
    {
        if (std::abs(it->first.first - cx) > m_Radius || std::abs(it->first.second - cy) > m_Radius)
        {
            it = m_Chunks.erase(it);
        }
        else
        {
            ++it;
        }
    }
    for (auto y = cy - m_Radius; y <= cy + m_Radius; ++y)
    {
        for (auto x = cx - m_Radius; x <= cx + m_Radius; ++x)
        {
            const ChunkKey key{x, y};
            if (m_Chunks.find(key) != m_Chunks.end())
            {
                continue;
            }
            std::vector<Tile> tiles;
            tiles.reserve(ChunkSize * ChunkSize);
            for (int ty = 0; ty < ChunkSize; ++ty)
            {
                for (int tx = 0; tx < ChunkSize; ++tx)
                {
                    tiles.push_back(MakeTile(x * ChunkSize + tx, y * ChunkSize + ty));
                }
            }
            m_Chunks.emplace(key, std::move(tiles));
        }
    }
}

bool Prototype::Blocked(double x, double y) const
{
    // A reproducible query independent of whether a chunk is currently loaded.
    const double sx = NearestShrine(x);
    const double sy = NearestShrine(y);
    if (Distance(x - (sx - 3), y - (sy - 3)) < 2.15)
    {
        return true;
    }
    if (Distance(x - sx, y - sy) < 0.55)
    {
        return true;
    }
    const auto ix = static_cast<std::int64_t>(std::round(x));
    const auto iy = static_cast<std::int64_t>(std::round(y));
    for (auto ty = iy - 1; ty <= iy + 1; ++ty)
    {
        for (auto tx = ix - 1; tx <= ix + 1; ++tx)
        {
            const Tile t = MakeTile(tx, ty);
            const double radius = t.prop == 2 ? 0.62 : 0.36;
            if (t.prop && Distance(x - double(tx), y - double(ty)) < radius)
            {
                return true;
            }
        }
    }
    return false;
}

void Prototype::Move(double& x, double& y, double dx, double dy, float radius)
{
    const auto free = [this, radius](double px, double py)
    {
        return !Blocked(px - radius, py) && !Blocked(px + radius, py) && !Blocked(px, py - radius)
               && !Blocked(px, py + radius);
    };
    if (free(x + dx, y))
    {
        x += dx;
    }
    if (free(x, y + dy))
    {
        y += dy;
    }
}

void Prototype::Notice(const std::string& text)
{
    m_Message = text;
    m_MessageTime = 5;
}

void Prototype::Die()
{
    // Evaluate the pre-death state: reaching the threshold on this death
    // mutates the survivor; dying while already mutated leaves an echo.
    if (m_Corruption >= MutationThreshold)
    {
        const float echoHealth = MaxHealth() + m_Corruption;
        const float echoDamage = 12.0f + m_Corruption * 0.08f + (m_Level - 1) * 2.0f + m_WeaponRank;
        m_Echoes.push_back({m_NextEcho++,
                            m_X,
                            m_Y,
                            m_Corruption,
                            echoHealth,
                            true,
                            echoHealth,
                            echoDamage,
                            m_Level});
        Notice("과거의 네가 이 땅에 남았다. 그것은 네 힘을 기억한다.");
    }
    else
    {
        Notice(m_Corruption + 25 >= MutationThreshold
                   ? "몸이 괴물로 변했다. 성물만이 너를 되돌릴 수 있다. [R]"
                   : "죽음에서 돌아왔다. 오염 +25. 네 안의 힘이 강해진다.");
    }
    ++m_Deaths;
    m_Corruption = (std::min)(150, m_Corruption + 25);
    m_X = m_ShrineX + 2;
    m_Y = m_ShrineY + 1;
    m_CameraX = m_X;
    m_CameraY = m_Y;
    m_Health = MaxHealth();
    m_HitCooldown = 4;
    m_Projectiles.clear();
    m_EnemyProjectiles.clear();
    m_TargetKind = 0;
    m_ShotCooldown = 0;

    for (FarmEnemy& enemy : m_FarmEnemies)
    {
        enemy.x = enemy.homeX;
        enemy.y = enemy.homeY;
        enemy.health = enemy.maxHealth;
        enemy.attackWindup = 0;
        enemy.burstWindup = 0;
        enemy.burstCooldown = 5;
    }
}

void Prototype::Key(unsigned char key, bool down)
{
    key = static_cast<unsigned char>(std::tolower(key));
    const bool pressed = down && !m_Keys[key];
    m_Keys[key] = down;
    if (!pressed)
    {
        return;
    }
    if (key == 'h')
    {
        m_Help = !m_Help;
    }
    if (key == 'g')
    {
        m_Grid = !m_Grid;
    }
    if (key == '+' || key == '=')
    {
        m_Zoom = (std::min)(1.6f, m_Zoom + 0.1f);
    }
    if (key == '-')
    {
        m_Zoom = (std::max)(0.65f, m_Zoom - 0.1f);
    }
    if (key == 'k')
    {
        Die();
    }
    if (key == 'r')
    {
        if (m_Corruption < MutationThreshold)
        {
            Notice("아직 인간의 모습이다. 성물은 변이한 뒤에 사용할 수 있다.");
        }
        else if (m_Relics == 0)
        {
            Notice("남은 성물이 없다. 시연을 다시 시작하면 하나를 지급받는다.");
        }
        else
        {
            --m_Relics;
            m_Corruption = 0;
            Notice("성물이 인간의 모습을 되찾아 주었다. 네 과거는 여전히 남아 있다.");
        }
    }
    if (key == 'e')
    {
        if (Distance(m_X - m_ShrineX, m_Y - m_ShrineY) > 2.8)
        {
            Notice("돌길을 따라 성소로 향하라. 불 가까이에서 정화할 수 있다.");
        }
        else if (m_Corruption >= MutationThreshold)
        {
            Notice("이 몸은 불로 씻어낼 수 없다. 인간으로 돌아가려면 성물이 필요하다.");
        }
        else
        {
            m_Corruption = 0;
            m_Health = MaxHealth();
            Notice("불이 너를 품는다. 오염이 씻겨 나가고 생명력이 회복되었다.");
        }
    }
    if (key == 'e')
    {
        CompleteLevelOne();
    }

    if (key == 'f')
    {
        m_AutoFire = !m_AutoFire;
        Notice(m_AutoFire ? "자동 사격 켜짐. 사거리 안의 보이는 적을 공격합니다."
                          : "자동 사격 꺼짐. SPACE를 누르고 있으면 사격합니다.");
    }

    if (key == 'q')
    {
        if (m_Potions <= 0)
        {
            Notice("회복약이 없습니다. 붉은 회복 아이템을 찾거나 성소로 돌아가세요.");
        }
        else if (m_Health >= MaxHealth())
        {
            Notice("생명력이 가득 차 있습니다.");
        }
        else
        {
            --m_Potions;
            m_Health = (std::min)(MaxHealth(), m_Health + MaxHealth() * 0.45f);
            Notice("회복약 사용. 생명력을 회복했습니다.");
        }
    }

    if (key == 'u')
    {
        if (m_WeaponRank >= 10)
        {
            Notice("현재 무기는 최대 강화 단계입니다.");
        }
        else if (m_Shards < 3)
        {
            Notice("무기 강화에는 강화 파편 3개가 필요합니다.");
        }
        else
        {
            m_Shards -= 3;
            ++m_WeaponRank;
            Notice("무기 +" + std::to_string(m_WeaponRank)
                   + " 강화! 발사체 피해가 4 증가했습니다.");
        }
    }

    if (key == 't')
    {
        if (m_Corruption >= MutationThreshold)
        {
            Notice("괴물화한 몸은 정화석으로 되돌릴 수 없습니다. 성물 [R]이 필요합니다.");
        }
        else if (m_Corruption == 0 || m_PurificationStones == 0)
        {
            Notice(m_Corruption == 0 ? "씻어낼 오염이 없습니다." : "정화석이 없습니다.");
        }
        else
        {
            --m_PurificationStones;
            m_Corruption = (std::max)(0, m_Corruption - 25);
            Notice("정화석으로 오염 25를 씻어냈습니다. 오염으로 얻은 힘도 줄어듭니다.");
        }
    }
}

void Prototype::Update(float dt)
{
    m_Time += dt;
    m_Attack = (std::max)(0.0f, m_Attack - dt);
    m_HitCooldown = (std::max)(0.0f, m_HitCooldown - dt);
    m_MessageTime = (std::max)(0.0f, m_MessageTime - dt);

    const float right = float(m_Keys['d']) - float(m_Keys['a']);
    const float down = float(m_Keys['s']) - float(m_Keys['w']);
    const double dx = right + down;
    const double dy = down - right;
    const double length = Distance(dx, dy);
    m_Walking = length > 0;
    if (m_Walking)
    {
        Move(m_X, m_Y, dx / length * 3.7 * dt, dy / length * 3.7 * dt, 0.18f);
    }

    m_ShrineX = NearestShrine(m_X);
    m_ShrineY = NearestShrine(m_Y);
    UpdateLevelOne(dt);

    const double blend = 1.0 - std::exp(-8.0 * dt);
    m_CameraX += (m_X - m_CameraX) * blend;
    m_CameraY += (m_Y - m_CameraY) * blend;
    Stream();
}

Point Prototype::Project(double x, double y) const
{
    const double dx = x - m_CameraX;
    const double dy = y - m_CameraY;
    return {float((dx - dy) * 40 * m_Zoom + m_Width * 0.5),
            float((dx + dy) * 20 * m_Zoom + m_Height * 0.73)};
}

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

void Prototype::Actor(Renderer& r, Point p, int corruption, bool hostile)
{
    const bool monster = corruption >= MutationThreshold;
    const float z = m_Zoom * (monster ? 1.22f : 1.0f);
    const float bob = !hostile && m_Walking ? std::sin(m_Time * 13) * 2 * z : 0;
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
        (monster ? 1u : 0u) | (hostile ? 2u : 0u) | (!hostile && m_Attack > 0 ? 4u : 0u);
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
                if (m_Attack > 0)
                {
                    r.Ellipse({p.x, p.y - 24 * z},
                              3 * z,
                              3 * z,
                              Color(1, 0.78f, 0.35f).Radiance(4));
                }
            }
        });
}

void Prototype::Draw(Renderer& r)
{
    m_Width = r.Width();
    m_Height = r.Height();
    r.Begin(Color(0.065f, 0.095f, 0.10f));

    struct Object
    {
        double x;
        double y;
        int kind;
        unsigned seed;
        size_t index;
    };

    std::vector<const Tile*> ground;
    std::vector<Object> objects;
    for (const auto& chunk : m_Chunks)
    {
        for (const auto& tile : chunk.second)
        {
            const Point p = Project(double(tile.x), double(tile.y));
            if (p.x < -220 * m_Zoom || p.x > m_Width + 220 * m_Zoom || p.y < -80 * m_Zoom
                || p.y > m_Height + 250 * m_Zoom)
            {
                continue;
            }
            ground.push_back(&tile);
            if (tile.prop)
            {
                objects.push_back({double(tile.x), double(tile.y), tile.prop, tile.variant, 0});
            }
        }
    }
    std::sort(ground.begin(),
              ground.end(),
              [](const Tile* a, const Tile* b)
              {
                  if (a->x + a->y != b->x + b->y)
                  {
                      return a->x + a->y < b->x + b->y;
                  }
                  return a->x < b->x;
              });
    for (const Tile* t : ground)
    {
        const Point p = Project(double(t->x), double(t->y));
        const float v = 0.86f + float(t->variant % 100) * 0.002f;
        Color c = t->road ? Color(0.27f, 0.285f, 0.25f) : Color(0.16f, 0.215f, 0.20f);
        if (LevelOneCell(t->x, t->y) >= 0)
        {
            c = t->road ? Color(0.32f, 0.29f, 0.21f) : Color(0.23f, 0.24f, 0.17f);
        }
        if (!t->road && t->taint > 0.56f)
        {
            c = Color(0.20f, 0.155f, 0.22f);
        }
        const float w = 40 * m_Zoom + 0.35f;
        const float h = 20 * m_Zoom + 0.25f;
        r.Quad({p.x, p.y - h}, {p.x + w, p.y}, {p.x, p.y + h}, {p.x - w, p.y}, c.Shade(v));
        if (t->road)
        {
            r.Line({p.x - w + 2, p.y},
                   {p.x, p.y + h - 1},
                   m_Zoom,
                   Color(0.10f, 0.14f, 0.14f, 0.5f));
            if (t->variant % 3 == 0)
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
                const float ox = float((t->variant >> (i * 5)) % 29) - 14;
                const float oy = float((t->variant >> (i * 4 + 2)) % 13) - 6;
                r.Line({p.x + ox * m_Zoom, p.y + oy * m_Zoom},
                       {p.x + (ox + 4) * m_Zoom, p.y + (oy - 2) * m_Zoom},
                       m_Zoom,
                       c.Shade(1.25f));
            }
            if (t->taint > 0.61f && t->variant % 4 == 0)
            {
                r.Ellipse(p, 24 * m_Zoom, 10 * m_Zoom, Color(0.12f, 0.065f, 0.16f, 0.8f));
                r.Line({p.x - 16 * m_Zoom, p.y},
                       {p.x + 8 * m_Zoom, p.y - 5 * m_Zoom},
                       m_Zoom,
                       Color(0.46f, 0.26f, 0.43f, 0.6f));
            }
        }
        if (m_Grid && (t->x % ChunkSize == 0 || t->y % ChunkSize == 0))
        {
            r.Line({p.x, p.y - h}, {p.x + w, p.y}, m_Zoom, Color(0.5f, 0.78f, 0.7f, 0.6f));
            r.Line({p.x, p.y - h}, {p.x - w, p.y}, m_Zoom, Color(0.5f, 0.78f, 0.7f, 0.6f));
        }
    }
    const double extent = m_Radius * ChunkSize;
    const auto minX = static_cast<std::int64_t>(std::floor((m_X - extent) / ShrineSpacing));
    const auto maxX = static_cast<std::int64_t>(std::ceil((m_X + extent) / ShrineSpacing));
    const auto minY = static_cast<std::int64_t>(std::floor((m_Y - extent) / ShrineSpacing));
    const auto maxY = static_cast<std::int64_t>(std::ceil((m_Y + extent) / ShrineSpacing));
    for (auto y = minY; y <= maxY; ++y)
    {
        for (auto x = minX; x <= maxX; ++x)
        {
            const double sx = double(x) * ShrineSpacing;
            const double sy = double(y) * ShrineSpacing;
            Point p = Project(sx, sy);
            if (p.x < -250 * m_Zoom || p.x > m_Width + 250 * m_Zoom || p.y < -100 * m_Zoom
                || p.y > m_Height + 400 * m_Zoom)
            {
                continue;
            }
            objects.push_back({sx - 3, sy - 3, 5, 0, 0});
            objects.push_back({sx, sy, 6, 0, 0});
        }
    }
    objects.push_back({m_X, m_Y, 7, 0, 0});
    for (size_t i = 0; i < m_FarmEnemies.size(); ++i)
    {
        const FarmEnemy& enemy = m_FarmEnemies[i];
        if (enemy.alive && Distance(enemy.x - m_X, enemy.y - m_Y) < extent)
        {
            objects.push_back({enemy.x, enemy.y, 10, 0, i});
        }
    }
    for (size_t i = 0; i < m_Echoes.size(); ++i)
    {
        const Echo& e = m_Echoes[i];
        if (Distance(e.x - m_X, e.y - m_Y) < extent)
        {
            objects.push_back({e.x, e.y, e.alive ? 8 : 9, 0, i});
        }
    }
    std::stable_sort(objects.begin(),
                     objects.end(),
                     [](const Object& a, const Object& b)
                     {
                         return a.x + a.y < b.x + b.y;
                     });
    for (const auto& o : objects)
    {
        const Point p = Project(o.x, o.y);
        if (o.kind == 1)
        {
            Tree(r, p, o.seed);
        }
        if (o.kind == 2)
        {
            Block(r, p, 24, 12, 37, Color(0.29f, 0.33f, 0.30f));
            Block(r, {p.x - 9 * m_Zoom, p.y - 35 * m_Zoom}, 12, 6, 14, Color(0.34f, 0.37f, 0.32f));
        }
        if (o.kind == 3)
        {
            Block(r,
                  p,
                  12 + float(o.seed % 8),
                  9,
                  10 + float(o.seed % 11),
                  Color(0.25f, 0.29f, 0.28f));
        }
        if (o.kind == 4)
        {
            Block(r, p, 10, 5, 7, Color(0.27f, 0.30f, 0.28f));
            r.Line({p.x, p.y - 5 * m_Zoom},
                   {p.x, p.y - 38 * m_Zoom},
                   5 * m_Zoom,
                   Muted.Shade(0.65f));
            r.Line({p.x - 9 * m_Zoom, p.y - 28 * m_Zoom},
                   {p.x + 9 * m_Zoom, p.y - 28 * m_Zoom},
                   4 * m_Zoom,
                   Muted.Shade(0.65f));
        }
        if (o.kind == 5)
        {
            Chapel(r, p);
        }
        if (o.kind == 6)
        {
            Shrine(r, p);
        }
        if (o.kind == 7)
        {
            Actor(r, p, m_Corruption, false);
        }
        if (o.kind == 8)
        {
            const Echo& e = m_Echoes[o.index];
            Actor(r, p, e.corruption, true);
        }
        if (o.kind == 9)
        {
            r.Ellipse(p, 16 * m_Zoom, 7 * m_Zoom, Color(0.28f, 0.10f, 0.25f, 0.75f));
        }
        if (o.kind == 10)
        {
            DrawFarmEnemy(r, p, m_FarmEnemies[o.index]);
        }
    }
    DrawCombatEffects(r);
    // Slow translucent haze and drifting ash; keep the center readable.
    for (int i = 0; i < 6; ++i)
    {
        const float x = std::fmod(i * 311.0f + m_Time * 9, m_Width + 600.0f) - 300;
        const float y = float(i % 3) * m_Height / 3.0f + 80;
        r.Glow({x, y}, 360, 90, Color(0.39f, 0.49f, 0.49f, 0.028f));
    }
    for (int i = 0; i < 45; ++i)
    {
        const float x = std::fmod(float(i * 137) + m_Time * (8 + i % 5), float(m_Width));
        const float y = std::fmod(float(i * 83) + m_Time * (3 + i % 3), float(m_Height));
        r.Rect(x, y, 1.5f, 1.5f, Color(0.68f, 0.72f, 0.66f, 0.20f));
    }
    // Finish scene rendering and composite HDR effects before any screen-space UI.
    r.BeginUI();
    Interface(r);
    r.Flush();
}

void Prototype::Interface(Renderer& r)
{
    const float w = float(m_Width);
    const float h = float(m_Height);
    DrawLevelOneUI(r);
    for (const Echo& e : m_Echoes)
    {
        if (!e.alive)
        {
            continue;
        }
        const Point p = Project(e.x, e.y);
        if (p.x < -80 || p.x > w + 80 || p.y < 0 || p.y > h + 140)
        {
            continue;
        }
        r.Rect(p.x - 25 * m_Zoom, p.y - 89 * m_Zoom, 50 * m_Zoom, 3 * m_Zoom, Ink);
        r.Rect(p.x - 25 * m_Zoom,
               p.y - 89 * m_Zoom,
               50 * m_Zoom * e.health / e.maxHealth,
               3 * m_Zoom,
               Violet);
        const std::string name =
            "잔재 " + std::to_string(e.id) + " · 레벨 " + std::to_string(e.level);
        r.Text(p.x - r.TextWidth(name, m_Zoom) * 0.5f, p.y - 109 * m_Zoom, name, Violet, m_Zoom);
    }
    r.Rect(0, 0, w, 105, Color(0.025f, 0.035f, 0.042f, 0.92f));
    r.Rect(28, 24, 3, 49, Gold);
    r.Text(45, 17, "마지막 불씨", Paper, 2.3f);
    r.Text(46, 53, "레벨 1 · 버려진 경작지", Muted, 1.4f);
    r.Text(46, 78, "불을 찾아라. 네가 누구인지 잊지 마라.", Gold, 1.2f);
    const float panelX = w - 305;
    r.Text(panelX,
           16,
           m_Corruption >= MutationThreshold ? "상태 / 괴물화" : "상태 / 인간",
           m_Corruption >= MutationThreshold ? Violet : Gold,
           1.6f);
    r.Text(panelX,
           40,
           std::to_string(int(m_Health)) + "/" + std::to_string(int(MaxHealth())),
           Muted,
           1.1f);
    r.Rect(panelX + 83, 43, 180, 7, Color(0.14f, 0.16f, 0.16f));
    r.Rect(panelX + 83, 43, 180 * m_Health / MaxHealth(), 7, Color(0.58f, 0.29f, 0.24f));
    r.Text(panelX, 59, "오염 " + std::to_string(m_Corruption), Paper, 1.2f);
    r.Rect(panelX, 80, 263, 5, Color(0.15f, 0.16f, 0.17f));
    r.Rect(panelX, 80, 263 * m_Corruption / 150.0f, 5, Violet);
    r.Rect(panelX + 263 * 100 / 150.0f, 76, 2, 13, Gold);
    if (m_Help)
    {
        r.Rect(24, 125, 255, 290, Color(0.025f, 0.035f, 0.042f, 0.86f));
        r.Text(40, 139, "파밍과 성장", Gold, 1.5f);
        r.Text(
            40,
            171,
            "WASD   이동\nSPACE 유지   자동 조준\nF   자동 사격 전환\nU   파편 3개로 무기 강화\nQ  "
            " 회복약 사용\nT   정화석 사용\nE   성소 회복·보고\nR   성물 사용\nH   안내 숨기기",
            Paper,
            1.4f);
        const auto& post = r.Post().settings;
        const auto onOff = [](bool value)
        {
            return value ? "켜짐" : "꺼짐";
        };
        std::ostringstream effects;
        effects << "P   전체 후처리: " << onOff(post.enabled) << "\nB   블룸: " << onOff(post.bloom)
                << "\nV   비네트: " << onOff(post.vignette)
                << "\nN   가장자리 블러: " << onOff(post.edgeBlur)
                << "\n[ / ]   노출: " << std::fixed << std::setprecision(2) << post.exposure
                << "\n0   효과 기본값 복원";
        r.Rect(w - 280, 125, 255, 205, Color(0.025f, 0.035f, 0.042f, 0.86f));
        r.Text(w - 264, 139, "빛과 그림자", Gold, 1.5f);
        r.Text(w - 264,
               171,
               r.Post().Available()
                   ? effects.str()
                   : "후처리 초기화 실패\n기본 렌더링으로 실행 중\n콘솔 오류를 확인해 주세요.",
               Paper,
               1.3f);
    }
    const Point fire = Project(m_ShrineX, m_ShrineY);
    if (Distance(m_X - m_ShrineX, m_Y - m_ShrineY) < 2.8)
    {
        const std::string text =
            m_Corruption >= MutationThreshold ? "성물이 필요하다 [R]" : "[E] 휴식과 정화";
        r.Text(fire.x - r.TextWidth(text, 1.4f) * 0.5f, fire.y + 38 * m_Zoom, text, Gold, 1.4f);
    }
    if (m_MessageTime > 0)
    {
        float scale = 1.5f;
        while (scale > 0.7f && r.TextWidth(m_Message, scale) > w - 70)
        {
            scale -= 0.05f;
        }
        const float length = r.TextWidth(m_Message, scale);
        r.Rect((w - length) * 0.5f - 15,
               h - 113,
               length + 30,
               39,
               Color(0.025f, 0.035f, 0.042f, 0.93f));
        r.Text((w - length) * 0.5f, h - 105, m_Message, Paper, scale);
    }
    r.Rect(0, h - 57, w, 57, Color(0.025f, 0.035f, 0.042f, 0.94f));
    r.Line({27, h - 57}, {w - 27, h - 57}, 1, Gold.Shade(0.45f));
    std::ostringstream stats;
    stats << "무기 +" << m_WeaponRank << " · 피해 " << int(ProjectileDamage()) << " · 발사 간격 "
          << std::fixed << std::setprecision(2) << ProjectileCooldown() << "초";
    r.Text(28, h - 46, stats.str(), Paper, 1.3f);
    r.Text(28,
           h - 21,
           "파편 " + std::to_string(m_Shards) + " · 회복약(Q) " + std::to_string(m_Potions)
               + " · 정화석(T) " + std::to_string(m_PurificationStones) + " · 성물(R) "
               + std::to_string(m_Relics),
           Muted,
           1.1f);
    if (m_Grid)
    {
        std::ostringstream status;
        status << "시드 " << m_WorldSeed;
        r.Text(w - 370, h - 36, status.str(), Gold, 1.1f);
    }
    else
    {
        r.Text(w - 322,
               h - 36,
               "성소까지 " + std::to_string(int(Distance(m_X - m_ShrineX, m_Y - m_ShrineY)))
                   + " 걸음",
               Gold,
               1.3f);
    }
}
