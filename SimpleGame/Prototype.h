#pragma once
#include "Renderer.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

class Prototype {
public:
    void Update(float dt);
    void Draw(Renderer& renderer);
    void Key(unsigned char key, bool down);
private:
    struct Tile { std::int64_t x, y; float taint; unsigned variant; int prop; bool road; };
    struct Echo {
        std::uint64_t id;
        double x, y;
        int corruption;
        float health;
        bool alive;
    };
    using ChunkKey = std::pair<std::int64_t, std::int64_t>;
    void Stream();
    Tile MakeTile(std::int64_t x, std::int64_t y) const;
    bool Blocked(double x, double y) const;
    void Move(double& x, double& y, double dx, double dy, float radius);
    void Die();
    void Notice(const std::string& text);
    Point Project(double x, double y) const;
    void Block(Renderer& r, Point p, float width, float depth, float height, Color color);
    void Tree(Renderer& r, Point p, unsigned seed);
    void Chapel(Renderer& r, Point p);
    void Shrine(Renderer& r, Point p);
    void Actor(Renderer& r, Point p, int corruption, bool hostile);
    void Interface(Renderer& r);
    std::map<ChunkKey, std::vector<Tile>> m_Chunks;
    std::vector<Echo> m_Echoes;
    bool m_Keys[256] = {};
    double m_X = 3, m_Y = 4, m_CameraX = 3, m_CameraY = 4;
    double m_ShrineX = 0, m_ShrineY = 0;
    float m_Time = 0, m_Zoom = 1.0f, m_Health = 100;
    float m_HitCooldown = 0, m_Attack = 0, m_MessageTime = 10;
    int m_Corruption = 0, m_Deaths = 0, m_Relics = 1;
    int m_Width = 1280, m_Height = 800, m_Radius = 3;
    bool m_Grid = false, m_Help = true, m_Walking = false;
    std::uint64_t m_NextEcho = 1;
    std::string m_Message = "마지막 불씨가 아직 타오른다. 성소를 찾아라.";
};
