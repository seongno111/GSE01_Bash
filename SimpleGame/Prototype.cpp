#include "stdafx.h"
#include "Prototype.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <sstream>

namespace {
    constexpr int ChunkSize = 8;
    constexpr int ShrineSpacing = 32;
    constexpr int MutationThreshold = 100;
    const Color Ink(0.025f, 0.035f, 0.042f);
    const Color Gold(0.84f, 0.65f, 0.36f);
    const Color Paper(0.78f, 0.79f, 0.73f);
    const Color Muted(0.43f, 0.52f, 0.51f);
    const Color Violet(0.65f, 0.30f, 0.65f);

    std::uint64_t Hash(std::int64_t x, std::int64_t y) {
        std::uint64_t n = std::uint64_t(x) * 0x9E3779B185EBCA87ULL
            ^ std::uint64_t(y) * 0xC2B2AE3D27D4EB4FULL ^ 0xA5B357019ULL;
        n ^= n >> 30; n *= 0xBF58476D1CE4E5B9ULL;
        n ^= n >> 27; n *= 0x94D049BB133111EBULL;
        return n ^ (n >> 31);
    }
    float Noise(std::int64_t x, std::int64_t y) {
        return float(Hash(x, y) & 65535) / 65535.0f;
    }
    float SmoothNoise(double x, double y) {
        const auto ix = static_cast<std::int64_t>(std::floor(x));
        const auto iy = static_cast<std::int64_t>(std::floor(y));
        float fx = float(x - ix), fy = float(y - iy);
        fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
        const float a = Noise(ix, iy) * (1-fx) + Noise(ix+1, iy) * fx;
        const float b = Noise(ix, iy+1) * (1-fx) + Noise(ix+1, iy+1) * fx;
        return a * (1-fy) + b * fy;
    }
    double Distance(double x, double y) { return std::sqrt(x*x+y*y); }
    std::int64_t ChunkOf(double p) { return static_cast<std::int64_t>(std::floor(p / ChunkSize)); }
    double NearestShrine(double p) { return std::round(p / ShrineSpacing) * ShrineSpacing; }
}

Prototype::Tile Prototype::MakeTile(std::int64_t x, std::int64_t y) const {
    const auto seed = Hash(x, y);
    const double lx = double(x) - NearestShrine(double(x));
    const double ly = double(y) - NearestShrine(double(y));
    const bool plaza = std::abs(lx) <= 3 && std::abs(ly) <= 3;
    const bool road = lx == 0 || ly == 0 || plaza;
    float taint = SmoothNoise(double(x) / 7.0 + 17, double(y) / 7.0 - 29);
    if (plaza) taint *= 0.25f;
    int prop = 0;
    // Keep the landmark and its approach clear, including the respawn point.
    if (!road && !(std::abs(lx) < 6 && std::abs(ly) < 6)) {
        if (seed % 19 == 0) prop = 1;       // dead tree
        else if (seed % 29 == 0) prop = 2;  // broken wall
        else if (seed % 13 == 0) prop = 3;  // rock
        else if (seed % 37 == 0) prop = 4;  // grave marker
    }
    return {x, y, taint, static_cast<unsigned>(seed >> 32), prop, road};
}

void Prototype::Stream() {
    const auto cx = ChunkOf(m_X), cy = ChunkOf(m_Y);
    // Account for all four screen corners in the inverse isometric projection.
    m_Radius = int(std::ceil((m_Width / 160.0 + m_Height / 80.0 + 8) / m_Zoom / ChunkSize)) + 1;
    for (auto it = m_Chunks.begin(); it != m_Chunks.end();) {
        if (std::abs(it->first.first-cx) > m_Radius || std::abs(it->first.second-cy) > m_Radius)
            it = m_Chunks.erase(it);
        else ++it;
    }
    for (auto y = cy-m_Radius; y <= cy+m_Radius; ++y) {
        for (auto x = cx-m_Radius; x <= cx+m_Radius; ++x) {
            const ChunkKey key{x,y};
            if (m_Chunks.find(key) != m_Chunks.end()) continue;
            std::vector<Tile> tiles;
            tiles.reserve(ChunkSize * ChunkSize);
            for (int ty = 0; ty < ChunkSize; ++ty)
                for (int tx = 0; tx < ChunkSize; ++tx)
                    tiles.push_back(MakeTile(x*ChunkSize+tx, y*ChunkSize+ty));
            m_Chunks.emplace(key, std::move(tiles));
        }
    }
}

bool Prototype::Blocked(double x, double y) const {
    // A reproducible query independent of whether a chunk is currently loaded.
    const double sx = NearestShrine(x), sy = NearestShrine(y);
    if (Distance(x-(sx-3), y-(sy-3)) < 2.15) return true;
    if (Distance(x-sx, y-sy) < 0.55) return true;
    const auto ix = static_cast<std::int64_t>(std::round(x));
    const auto iy = static_cast<std::int64_t>(std::round(y));
    for (auto ty = iy-1; ty <= iy+1; ++ty)
        for (auto tx = ix-1; tx <= ix+1; ++tx) {
            const Tile t = MakeTile(tx, ty);
            const double radius = t.prop == 2 ? 0.62 : 0.36;
            if (t.prop && Distance(x-double(tx), y-double(ty)) < radius) return true;
        }
    return false;
}

void Prototype::Move(double& x, double& y, double dx, double dy, float radius) {
    const auto free = [this, radius](double px, double py) {
        return !Blocked(px-radius, py) && !Blocked(px+radius, py)
            && !Blocked(px, py-radius) && !Blocked(px, py+radius);
    };
    if (free(x+dx, y)) x += dx;
    if (free(x, y+dy)) y += dy;
}

void Prototype::Notice(const std::string& text) {
    m_Message = text; m_MessageTime = 5;
}

void Prototype::Die() {
    // Evaluate the pre-death state: reaching the threshold on this death
    // mutates the survivor; dying while already mutated leaves an echo.
    if (m_Corruption >= MutationThreshold) {
        m_Echoes.push_back({m_NextEcho++, m_X, m_Y, m_Corruption,
            100.0f + m_Corruption, true});
        Notice("과거의 네가 이 땅에 남았다. 그것은 네 힘을 기억한다.");
    } else {
        Notice(m_Corruption + 25 >= MutationThreshold
            ? "몸이 괴물로 변했다. 성물만이 너를 되돌릴 수 있다. [R]"
            : "죽음에서 돌아왔다. 오염 +25. 네 안의 힘이 강해진다.");
    }
    ++m_Deaths;
    m_Corruption = (std::min)(150, m_Corruption + 25);
    m_X = m_ShrineX + 3; m_Y = m_ShrineY + 4;
    m_CameraX = m_X; m_CameraY = m_Y;
    m_Health = 100; m_HitCooldown = 4;
}

void Prototype::Key(unsigned char key, bool down) {
    key = static_cast<unsigned char>(std::tolower(key));
    const bool pressed = down && !m_Keys[key];
    m_Keys[key] = down;
    if (!pressed) return;
    if (key == 'h') m_Help = !m_Help;
    if (key == 'g') m_Grid = !m_Grid;
    if (key == '+' || key == '=') m_Zoom = (std::min)(1.6f, m_Zoom + 0.1f);
    if (key == '-') m_Zoom = (std::max)(0.65f, m_Zoom - 0.1f);
    if (key == 'k') Die();
    if (key == 'r') {
        if (m_Corruption < MutationThreshold) Notice("아직 인간의 모습이다. 성물은 변이한 뒤에 사용할 수 있다.");
        else if (m_Relics == 0) Notice("남은 성물이 없다. 시연을 다시 시작하면 하나를 지급받는다.");
        else { --m_Relics; m_Corruption = 0; Notice("성물이 인간의 모습을 되찾아 주었다. 네 과거는 여전히 남아 있다."); }
    }
    if (key == 'e') {
        if (Distance(m_X-m_ShrineX, m_Y-m_ShrineY) > 2.8) Notice("돌길을 따라 성소로 향하라. 불 가까이에서 정화할 수 있다.");
        else if (m_Corruption >= MutationThreshold) Notice("이 몸은 불로 씻어낼 수 없다. 인간으로 돌아가려면 성물이 필요하다.");
        else { m_Corruption = 0; m_Health = 100; Notice("불이 너를 품는다. 오염이 씻겨 나가고 생명력이 회복되었다."); }
    }
    if (key == ' ' && m_Attack <= 0) {
        m_Attack = 0.45f;
        for (auto& echo : m_Echoes) {
            if (echo.alive && Distance(echo.x-m_X, echo.y-m_Y) < 1.8) {
                echo.health -= 24 + m_Corruption * 0.3f;
                if (echo.health <= 0) { echo.alive = false; Notice("잔재가 쓰러졌다. 그 흔적은 이 땅에 남는다."); }
            }
        }
    }
}

void Prototype::Update(float dt) {
    m_Time += dt;
    m_Attack = (std::max)(0.0f, m_Attack-dt);
    m_HitCooldown = (std::max)(0.0f, m_HitCooldown-dt);
    m_MessageTime = (std::max)(0.0f, m_MessageTime-dt);
    const float right = float(m_Keys['d']) - float(m_Keys['a']);
    const float down = float(m_Keys['s']) - float(m_Keys['w']);
    double dx = right+down, dy = down-right;
    const double length = Distance(dx,dy);
    m_Walking = length > 0;
    if (m_Walking) Move(m_X, m_Y, dx/length*3.7*dt, dy/length*3.7*dt, 0.18f);
    m_ShrineX = NearestShrine(m_X); m_ShrineY = NearestShrine(m_Y);
    bool killed = false;
    for (auto& echo : m_Echoes) {
        if (!echo.alive) continue;
        dx = m_X-echo.x; dy = m_Y-echo.y;
        const double distance = Distance(dx,dy);
        const bool sanctuary = Distance(m_X-m_ShrineX,m_Y-m_ShrineY) < 2.8;
        if (distance > 0.7 && distance < 12 && !sanctuary)
            Move(echo.x, echo.y, dx/distance*2.0*dt, dy/distance*2.0*dt, 0.18f);
        if (distance < 1.0 && !sanctuary && m_HitCooldown <= 0) {
            m_Health -= 12 + echo.corruption * 0.08f;
            m_HitCooldown = 1;
            if (m_Health <= 0) { killed = true; break; }
        }
    }
    // Appending to the echo vector happens only after iteration has ended.
    if (killed) Die();
    const double blend = 1.0 - std::exp(-8.0*dt);
    m_CameraX += (m_X-m_CameraX)*blend; m_CameraY += (m_Y-m_CameraY)*blend;
    Stream();
}

Point Prototype::Project(double x, double y) const {
    const double dx = x-m_CameraX, dy = y-m_CameraY;
    return {float((dx-dy)*40*m_Zoom + m_Width*0.5),
        float((dx+dy)*20*m_Zoom + m_Height*0.73)};
}

void Prototype::Block(Renderer& r, Point p, float width, float depth, float height, Color c) {
    const float w = width*m_Zoom, d = depth*m_Zoom, h = height*m_Zoom;
    const Point top{p.x,p.y-d-h}, left{p.x-w,p.y-h}, right{p.x+w,p.y-h}, bottom{p.x,p.y+d-h};
    r.Quad(left,bottom,{p.x,p.y+d},{p.x-w,p.y},c.Shade(0.63f));
    r.Quad(bottom,right,{p.x+w,p.y},{p.x,p.y+d},c.Shade(0.43f));
    r.Quad(top,right,bottom,left,c);
    r.Line(left,bottom,1*m_Zoom,c.Shade(1.15f));
}

void Prototype::Tree(Renderer& r, Point p, unsigned seed) {
    const float z = m_Zoom, h = (65 + seed%40)*z;
    const Color bark(0.15f,0.19f,0.18f);
    r.Ellipse({p.x+16*z,p.y+6*z},33*z,9*z,Color(0.01f,0.02f,0.02f,0.38f));
    r.Triangle({p.x-6*z,p.y},{p.x+5*z,p.y},{p.x+3*z,p.y-h},bark);
    r.Line({p.x,p.y-h*0.45f},{p.x-23*z,p.y-h*0.72f},4*z,bark);
    r.Line({p.x-23*z,p.y-h*0.72f},{p.x-29*z,p.y-h*0.94f},2*z,bark);
    r.Line({p.x-20*z,p.y-h*0.69f},{p.x-40*z,p.y-h*0.73f},2*z,bark);
    r.Line({p.x+1*z,p.y-h*0.65f},{p.x+25*z,p.y-h*0.85f},3*z,bark.Shade(0.8f));
    r.Line({p.x+25*z,p.y-h*0.85f},{p.x+27*z,p.y-h*1.04f},2*z,bark);
    r.Line({p.x+2*z,p.y-h*0.9f},{p.x-8*z,p.y-h*1.12f},2*z,bark);
    r.Line({p.x-2*z,p.y-10*z},{p.x-1*z,p.y-h*0.63f},z,Color(0.31f,0.34f,0.28f));
}

void Prototype::Chapel(Renderer& r, Point p) {
    const float z = m_Zoom;
    r.Ellipse({p.x+45*z,p.y+15*z},135*z,31*z,Color(0.01f,0.02f,0.025f,0.45f));
    Block(r,p,99,45,12,Color(0.28f,0.30f,0.28f));
    Block(r,{p.x,p.y-10*z},73,32,77,Color(0.32f,0.35f,0.33f));
    // Broad facade with an exposed, fractured gable.
    r.Quad({p.x-53*z,p.y-9*z},{p.x+38*z,p.y+7*z},
        {p.x+38*z,p.y-93*z},{p.x-53*z,p.y-108*z},Color(0.24f,0.28f,0.27f));
    r.Triangle({p.x-53*z,p.y-108*z},{p.x-8*z,p.y-164*z},
        {p.x+38*z,p.y-93*z},Color(0.29f,0.33f,0.30f));
    r.Triangle({p.x-8*z,p.y-164*z},{p.x+66*z,p.y-185*z},
        {p.x+38*z,p.y-93*z},Color(0.12f,0.18f,0.19f));
    for (int i = 0; i < 5; ++i)
        r.Line({p.x-49*z,p.y-(18+i*16)*z},{p.x+35*z,p.y-(3+i*16)*z},z,Color(0.14f,0.19f,0.19f));
    // Pointed doorway, window and cross are deliberately readable silhouettes.
    r.Rect(p.x-20*z,p.y-48*z,25*z,41*z,Ink);
    r.Triangle({p.x-20*z,p.y-48*z},{p.x-7*z,p.y-68*z},{p.x+5*z,p.y-48*z},Ink);
    r.Glow({p.x-7*z,p.y-27*z},22*z,39*z,Color(0.94f,0.45f,0.13f,0.32f));
    r.Ellipse({p.x-8*z,p.y-102*z},12*z,18*z,Ink);
    r.Ellipse({p.x-8*z,p.y-102*z},7*z,13*z,Color(0.60f,0.38f,0.15f));
    r.Line({p.x-8*z,p.y-118*z},{p.x-8*z,p.y-86*z},3*z,Ink);
    r.Line({p.x-20*z,p.y-103*z},{p.x+3*z,p.y-101*z},3*z,Ink);
    Block(r,{p.x-63*z,p.y-7*z},15,9,132,Color(0.34f,0.37f,0.33f));
    Block(r,{p.x+52*z,p.y+7*z},15,9,108,Color(0.31f,0.34f,0.30f));
    r.Triangle({p.x-80*z,p.y-139*z},{p.x-63*z,p.y-184*z},
        {p.x-46*z,p.y-139*z},Color(0.13f,0.19f,0.19f));
    r.Line({p.x-63*z,p.y-209*z},{p.x-63*z,p.y-181*z},3*z,Gold.Shade(0.65f));
    r.Line({p.x-73*z,p.y-200*z},{p.x-53*z,p.y-200*z},3*z,Gold.Shade(0.65f));
    Block(r,{p.x+52*z,p.y-101*z},13,8,11,Color(0.39f,0.40f,0.34f));
    r.Line({p.x+17*z,p.y-86*z},{p.x+10*z,p.y-68*z},2*z,Ink);
    r.Line({p.x+10*z,p.y-68*z},{p.x+19*z,p.y-57*z},2*z,Ink);
    Block(r,{p.x+78*z,p.y+36*z},17,9,9,Color(0.29f,0.31f,0.28f));
}

void Prototype::Shrine(Renderer& r, Point p) {
    const float z = m_Zoom;
    r.Ellipse(p,70*z,33*z,Color(0.52f,0.37f,0.18f,0.13f));
    Block(r,p,30,15,7,Color(0.43f,0.40f,0.31f));
    Block(r,{p.x,p.y-7*z},17,8,20,Color(0.37f,0.33f,0.25f));
    r.Ellipse({p.x,p.y-29*z},21*z,8*z,Color(0.16f,0.17f,0.14f));
    const float flicker = std::sin(m_Time*9)*3;
    r.Glow({p.x,p.y-42*z},70*z,88*z,Color(1,0.46f,0.12f,0.19f));
    r.Triangle({p.x-13*z,p.y-29*z},{p.x+(3+flicker)*z,p.y-73*z},
        {p.x+14*z,p.y-29*z},Color(0.95f,0.39f,0.10f));
    r.Triangle({p.x-7*z,p.y-29*z},{p.x-flicker*z,p.y-58*z},
        {p.x+7*z,p.y-29*z},Color(1,0.80f,0.34f));
    for (int i = 0; i < 7; ++i) {
        const float age = std::fmod(m_Time*0.35f+i*0.143f,1.0f);
        r.Rect(p.x+std::sin(i*8.0f+age*5)*12*z,p.y-(32+age*77)*z,
            2*z,2*z,Color(1,0.69f,0.23f,1-age));
    }
}

void Prototype::Actor(Renderer& r, Point p, int corruption, bool hostile) {
    const bool monster = corruption >= MutationThreshold;
    const float z = m_Zoom*(monster ? 1.22f : 1.0f);
    const float bob = !hostile && m_Walking ? std::sin(m_Time*13)*2*z : 0;
    const Color cloak = hostile ? Color(0.30f,0.10f,0.23f)
        : monster ? Color(0.28f,0.19f,0.33f) : Color(0.28f,0.34f,0.35f);
    r.Ellipse({p.x,p.y+2*z},17*z,7*z,Color(0.005f,0.01f,0.02f,0.55f));
    if (!hostile) {
        const Color ring = monster ? Violet : Gold;
        for (int i = 0; i < 24; ++i) {
            const float a = i*6.2831853f/24, b = (i+1)*6.2831853f/24;
            r.Line({p.x+19*z*std::cos(a),p.y+9*z*std::sin(a)},
                {p.x+19*z*std::cos(b),p.y+9*z*std::sin(b)},z,ring);
        }
    }
    p.y += bob;
    r.Line({p.x-5*z,p.y-12*z},{p.x-6*z,p.y},5*z,Ink);
    r.Line({p.x+5*z,p.y-12*z},{p.x+7*z,p.y},5*z,Ink);
    r.Triangle({p.x,p.y-39*z},{p.x-16*z,p.y-7*z},{p.x+15*z,p.y-7*z},cloak);
    r.Triangle({p.x,p.y-39*z},{p.x+3*z,p.y-7*z},{p.x+15*z,p.y-7*z},cloak.Shade(0.58f));
    r.Ellipse({p.x,p.y-39*z},10*z,12*z,cloak.Shade(1.35f));
    r.Quad({p.x-6*z,p.y-42*z},{p.x+6*z,p.y-42*z},
        {p.x+4*z,p.y-34*z},{p.x-4*z,p.y-34*z},Ink);
    r.Rect(p.x-4*z,p.y-40*z,3*z,z,monster ? Color(1,0.3f,0.55f) : Gold);
    r.Rect(p.x+2*z,p.y-40*z,3*z,z,monster ? Color(1,0.3f,0.55f) : Gold);
    if (monster) {
        r.Triangle({p.x-8*z,p.y-43*z},{p.x-18*z,p.y-61*z},{p.x-6*z,p.y-50*z},Violet.Shade(0.65f));
        r.Triangle({p.x+8*z,p.y-43*z},{p.x+17*z,p.y-62*z},{p.x+6*z,p.y-50*z},Violet.Shade(0.8f));
        r.Line({p.x+12*z,p.y-25*z},{p.x+23*z,p.y-9*z},4*z,cloak);
        r.Line({p.x+23*z,p.y-9*z},{p.x+20*z,p.y+1*z},2*z,Paper);
        r.Glow({p.x,p.y-24*z},32*z,43*z,Color(0.6f,0.12f,0.45f,0.09f));
    } else {
        r.Line({p.x+13*z,p.y-23*z},{p.x+20*z,p.y-3*z},3*z,Color(0.54f,0.60f,0.59f));
        r.Line({p.x+10*z,p.y-19*z},{p.x+19*z,p.y-23*z},3*z,Gold);
    }
    if (!hostile) {
        r.Line({p.x-12*z,p.y-20*z},{p.x-20*z,p.y-37*z},3*z,Color(0.38f,0.28f,0.17f));
        r.Glow({p.x-20*z,p.y-43*z},28*z,36*z,Color(1,0.50f,0.13f,0.32f));
        r.Triangle({p.x-24*z,p.y-38*z},{p.x-19*z,p.y-54*z},{p.x-15*z,p.y-38*z},Gold);
        if (m_Attack > 0.23f) {
            for (int i = 0; i < 12; ++i) {
                const float a = float(i)*0.19f-1.0f, b = a+0.19f;
                r.Line({p.x+52*z*std::cos(a),p.y-15*z+25*z*std::sin(a)},
                    {p.x+52*z*std::cos(b),p.y-15*z+25*z*std::sin(b)},3*z,Paper);
            }
        }
    }
}

void Prototype::Draw(Renderer& r) {
    m_Width = r.Width(); m_Height = r.Height();
    r.Begin(Color(0.065f,0.095f,0.10f));
    struct Object { double x,y; int kind; unsigned seed; size_t index; };
    std::vector<const Tile*> ground;
    std::vector<Object> objects;
    for (const auto& chunk : m_Chunks)
        for (const auto& tile : chunk.second) {
            const Point p = Project(double(tile.x),double(tile.y));
            if (p.x < -220*m_Zoom || p.x > m_Width+220*m_Zoom
                || p.y < -80*m_Zoom || p.y > m_Height+250*m_Zoom) continue;
            ground.push_back(&tile);
            if (tile.prop) objects.push_back({double(tile.x),double(tile.y),tile.prop,tile.variant,0});
        }
    std::sort(ground.begin(),ground.end(),[](const Tile* a, const Tile* b) {
        if (a->x+a->y != b->x+b->y) return a->x+a->y < b->x+b->y;
        return a->x < b->x;
    });
    for (const Tile* t : ground) {
        const Point p = Project(double(t->x),double(t->y));
        const float v = 0.86f + float(t->variant%100)*0.002f;
        Color c = t->road ? Color(0.27f,0.285f,0.25f) : Color(0.16f,0.215f,0.20f);
        if (!t->road && t->taint > 0.56f) c = Color(0.20f,0.155f,0.22f);
        const float w = 40*m_Zoom+0.35f, h = 20*m_Zoom+0.25f;
        r.Quad({p.x,p.y-h},{p.x+w,p.y},{p.x,p.y+h},{p.x-w,p.y},c.Shade(v));
        if (t->road) {
            r.Line({p.x-w+2,p.y},{p.x,p.y+h-1},m_Zoom,Color(0.10f,0.14f,0.14f,0.5f));
            if (t->variant%3 == 0)
                r.Line({p.x-9*m_Zoom,p.y-9*m_Zoom},{p.x+5*m_Zoom,p.y+8*m_Zoom},m_Zoom,c.Shade(0.6f));
        } else {
            for (int i = 0; i < 3; ++i) {
                const float ox = float((t->variant>>(i*5))%29)-14;
                const float oy = float((t->variant>>(i*4+2))%13)-6;
                r.Line({p.x+ox*m_Zoom,p.y+oy*m_Zoom},
                    {p.x+(ox+4)*m_Zoom,p.y+(oy-2)*m_Zoom},m_Zoom,c.Shade(1.25f));
            }
            if (t->taint > 0.61f && t->variant%4 == 0) {
                r.Ellipse(p,24*m_Zoom,10*m_Zoom,Color(0.12f,0.065f,0.16f,0.8f));
                r.Line({p.x-16*m_Zoom,p.y},{p.x+8*m_Zoom,p.y-5*m_Zoom},m_Zoom,Color(0.46f,0.26f,0.43f,0.6f));
            }
        }
        if (m_Grid && (t->x%ChunkSize == 0 || t->y%ChunkSize == 0)) {
            r.Line({p.x,p.y-h},{p.x+w,p.y},m_Zoom,Color(0.5f,0.78f,0.7f,0.6f));
            r.Line({p.x,p.y-h},{p.x-w,p.y},m_Zoom,Color(0.5f,0.78f,0.7f,0.6f));
        }
    }
    const double extent = m_Radius*ChunkSize;
    const auto minX = static_cast<std::int64_t>(std::floor((m_X-extent)/ShrineSpacing));
    const auto maxX = static_cast<std::int64_t>(std::ceil((m_X+extent)/ShrineSpacing));
    const auto minY = static_cast<std::int64_t>(std::floor((m_Y-extent)/ShrineSpacing));
    const auto maxY = static_cast<std::int64_t>(std::ceil((m_Y+extent)/ShrineSpacing));
    for (auto y = minY; y <= maxY; ++y)
        for (auto x = minX; x <= maxX; ++x) {
            const double sx = double(x)*ShrineSpacing, sy = double(y)*ShrineSpacing;
            Point p = Project(sx,sy);
            if (p.x < -250*m_Zoom || p.x > m_Width+250*m_Zoom || p.y < -100*m_Zoom || p.y > m_Height+400*m_Zoom) continue;
            r.Glow(p,190*m_Zoom,95*m_Zoom,Color(0.92f,0.55f,0.22f,0.23f));
            objects.push_back({sx-3,sy-3,5,0,0});
            objects.push_back({sx,sy,6,0,0});
        }
    r.Glow(Project(m_X,m_Y),105*m_Zoom,58*m_Zoom,Color(0.81f,0.55f,0.25f,0.10f));
    objects.push_back({m_X,m_Y,7,0,0});
    for (size_t i = 0; i < m_Echoes.size(); ++i) {
        const Echo& e = m_Echoes[i];
        if (Distance(e.x-m_X,e.y-m_Y) < extent)
            objects.push_back({e.x,e.y,e.alive ? 8 : 9,0,i});
    }
    std::stable_sort(objects.begin(),objects.end(),[](const Object& a,const Object& b) {
        return a.x+a.y < b.x+b.y;
    });
    for (const auto& o : objects) {
        const Point p = Project(o.x,o.y);
        if (o.kind == 1) Tree(r,p,o.seed);
        if (o.kind == 2) {
            Block(r,p,24,12,37,Color(0.29f,0.33f,0.30f));
            Block(r,{p.x-9*m_Zoom,p.y-35*m_Zoom},12,6,14,Color(0.34f,0.37f,0.32f));
        }
        if (o.kind == 3) Block(r,p,12+float(o.seed%8),9,10+float(o.seed%11),Color(0.25f,0.29f,0.28f));
        if (o.kind == 4) {
            Block(r,p,10,5,7,Color(0.27f,0.30f,0.28f));
            r.Line({p.x,p.y-5*m_Zoom},{p.x,p.y-38*m_Zoom},5*m_Zoom,Muted.Shade(0.65f));
            r.Line({p.x-9*m_Zoom,p.y-28*m_Zoom},{p.x+9*m_Zoom,p.y-28*m_Zoom},4*m_Zoom,Muted.Shade(0.65f));
        }
        if (o.kind == 5) Chapel(r,p);
        if (o.kind == 6) Shrine(r,p);
        if (o.kind == 7) Actor(r,p,m_Corruption,false);
        if (o.kind == 8) {
            const Echo& e = m_Echoes[o.index];
            Actor(r,p,e.corruption,true);
            r.Rect(p.x-25*m_Zoom,p.y-89*m_Zoom,50*m_Zoom,3*m_Zoom,Ink);
            r.Rect(p.x-25*m_Zoom,p.y-89*m_Zoom,50*m_Zoom*e.health/(100+e.corruption),3*m_Zoom,Violet);
            const std::string name = "잔재 " + std::to_string(e.id);
            r.Text(p.x-r.TextWidth(name,m_Zoom)*0.5f,p.y-109*m_Zoom,name,Violet,m_Zoom);
        }
        if (o.kind == 9) r.Ellipse(p,16*m_Zoom,7*m_Zoom,Color(0.28f,0.10f,0.25f,0.75f));
    }
    // Slow translucent haze and drifting ash; keep the center readable.
    for (int i = 0; i < 6; ++i) {
        const float x = std::fmod(i*311.0f+m_Time*9,m_Width+600.0f)-300;
        const float y = float(i%3)*m_Height/3.0f+80;
        r.Glow({x,y},360,90,Color(0.39f,0.49f,0.49f,0.028f));
    }
    for (int i = 0; i < 45; ++i) {
        const float x = std::fmod(float(i*137)+m_Time*(8+i%5),float(m_Width));
        const float y = std::fmod(float(i*83)+m_Time*(3+i%3),float(m_Height));
        r.Rect(x,y,1.5f,1.5f,Color(0.68f,0.72f,0.66f,0.20f));
    }
    for (int i = 0; i < 12; ++i) {
        const float alpha = 0.028f;
        r.Rect(float(i*9),0,9,float(m_Height),Color(0,0.015f,0.02f,alpha*(12-i)));
        r.Rect(float(m_Width-(i+1)*9),0,9,float(m_Height),Color(0,0.015f,0.02f,alpha*(12-i)));
    }
    Interface(r);
    r.Flush();
}

void Prototype::Interface(Renderer& r) {
    const float w = float(m_Width), h = float(m_Height);
    r.Rect(0,0,w,105,Color(0.025f,0.035f,0.042f,0.92f));
    r.Rect(28,24,3,49,Gold);
    r.Text(45,17,"마지막 불씨",Paper,2.3f);
    r.Text(46,53,"잿빛 변경 / 렌더링 프로토타입",Muted,1.4f);
    r.Text(46,78,"불을 찾아라. 네가 누구인지 잊지 마라.",Gold,1.2f);
    const float panelX = w-305;
    r.Text(panelX,16,m_Corruption >= MutationThreshold ? "상태 / 괴물화" : "상태 / 인간",m_Corruption >= MutationThreshold ? Violet : Gold,1.6f);
    r.Text(panelX,40,"생명력",Muted,1.2f);
    r.Rect(panelX+83,43,180,7,Color(0.14f,0.16f,0.16f));
    r.Rect(panelX+83,43,180*m_Health/100,7,Color(0.58f,0.29f,0.24f));
    r.Text(panelX,59,"오염 " + std::to_string(m_Corruption),Paper,1.2f);
    r.Rect(panelX,80,263,5,Color(0.15f,0.16f,0.17f));
    r.Rect(panelX,80,263*m_Corruption/150.0f,5,Violet);
    r.Rect(panelX+263*100/150.0f,76,2,13,Gold);
    if (m_Help) {
        r.Rect(24,125,255,290,Color(0.025f,0.035f,0.042f,0.86f));
        r.Text(40,139,"폐허 속의 빛",Gold,1.5f);
        r.Text(40,171,"WASD   이동\nSPACE   공격\nE   성소에서 회복·정화\nR   성물 사용\nK   사망 시연\nG   청크 경계 표시\n+/-   확대·축소\nH   안내 숨기기\nESC   종료",Paper,1.4f);
    }
    const Point fire = Project(m_ShrineX,m_ShrineY);
    if (Distance(m_X-m_ShrineX,m_Y-m_ShrineY) < 2.8) {
        const std::string text = m_Corruption >= MutationThreshold ? "성물이 필요하다 [R]" : "[E] 휴식과 정화";
        r.Text(fire.x-r.TextWidth(text,1.4f)*0.5f,fire.y+38*m_Zoom,text,Gold,1.4f);
    }
    if (m_MessageTime > 0) {
        float scale = 1.5f;
        while (scale > 0.7f && r.TextWidth(m_Message,scale) > w-70) scale -= 0.05f;
        const float length = r.TextWidth(m_Message,scale);
        r.Rect((w-length)*0.5f-15,h-113,length+30,39,Color(0.025f,0.035f,0.042f,0.93f));
        r.Text((w-length)*0.5f,h-105,m_Message,Paper,scale);
    }
    r.Rect(0,h-57,w,57,Color(0.025f,0.035f,0.042f,0.94f));
    r.Line({27,h-57},{w-27,h-57},1,Gold.Shade(0.45f));
    r.Text(28,h-46,"공격력 " + std::to_string(int(24+m_Corruption*0.3f))
        + "   성물 " + std::to_string(m_Relics) + "   사망 " + std::to_string(m_Deaths),Paper,1.5f);
    r.Text(28,h-21,"H: 조작 안내 / K: 사망 시연 / 종료 시 초기화",Muted,1.1f);
    if (m_Grid) {
        std::ostringstream status;
        status << "청크 " << ChunkOf(m_X) << "," << ChunkOf(m_Y)
            << " / 로드 " << m_Chunks.size();
        r.Text(w-370,h-36,status.str(),Gold,1.1f);
    } else {
        r.Text(w-322,h-36,"성소까지 " + std::to_string(int(Distance(m_X-m_ShrineX,m_Y-m_ShrineY))) + " 걸음",Gold,1.3f);
    }
}
