#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <cstdint>
#include <cstddef>
#include "Dependencies/glew.h"
#include "PostProcessing.h"

struct Point
{
    float x;
    float y;
};

struct Color
{
    float r;
    float g;
    float b;
    float a;
    float intensity = 1;

    Color(float red = 1, float green = 1, float blue = 1, float alpha = 1)
        : r(red),
          g(green),
          b(blue),
          a(alpha)
    {
    }

    Color Shade(float factor) const
    {
        return Color(r * factor, g * factor, b * factor, a);
    }

    // RGB is authored in sRGB; intensity multiplies it after linearization.
    Color Radiance(float value) const
    {
        Color result = *this;
        result.intensity = value;
        return result;
    }
};

// Pixel coordinates, top-left origin. Submission order defines painter ordering.
class Renderer
{
public:

    Renderer(int width, int height);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool IsInitialized() const
    {
        return m_Program != 0 && m_VBO != 0 && m_VAO != 0;
    }

    void Resize(int width, int height);
    void Begin(Color background);
    void Flush();
    void BeginUI();

    // The builder only submits geometry in the supplied coordinates and runs
    // on a cache miss. Nested builders become part of their parent's mesh.
    // Position/scale/tint are instance properties and must not be part of the key.
    // Include every baked shape/color/state variant in the key; do not animate
    // or mutate game state inside a builder. Invalidate after changing geometry.
    void DrawCachedMesh(const std::string& key,
                        Point position,
                        float scale,
                        const std::function<void()>& builder,
                        Color tint = Color());
    void InvalidateMesh(const std::string& key);
    size_t CachedMeshCount() const;
    size_t MeshCacheHits() const;
    size_t MeshCacheMisses() const;

    PostProcessing& Post()
    {
        return m_Post;
    }

    void Triangle(Point a, Point b, Point c, Color color);
    void Quad(Point a, Point b, Point c, Point d, Color color);
    void Rect(float x, float y, float width, float height, Color color);
    void Line(Point a, Point b, float width, Color color);
    void Ellipse(Point center, float rx, float ry, Color color, int segments = 24);
    void Glow(Point center, float rx, float ry, Color color);
    void Text(float x, float y, const std::string& text, Color color, float scale = 2);
    float TextWidth(const std::string& text, float scale = 2);

    int Width() const
    {
        return m_Width;
    }

    int Height() const
    {
        return m_Height;
    }

private:

    struct FontCache;
    std::unique_ptr<FontCache> m_Font;

    struct Vertex
    {
        float x;
        float y;
        float r;
        float g;
        float b;
        float a;
        float intensity;
    };

    void VertexAt(Point p, Color c);
    void ConfigureVertexArray(GLuint vao, GLuint vbo);
    const std::vector<Point>& UnitCircle(int segments);
    void TrimMeshCache();

    struct CachedMesh;

    struct CacheEntry
    {
        std::shared_ptr<CachedMesh> mesh;
        std::uint64_t lastUsed = 0;
    };

    struct DrawCommand
    {
        std::shared_ptr<CachedMesh> mesh;
        GLint first = 0;
        GLsizei count = 0;
        Point position{0, 0};
        float scale = 1;
        Color tint;
    };

    std::unordered_map<std::string, CacheEntry> m_MeshCache;
    std::unordered_map<int, std::vector<Point>> m_CircleCache;
    std::vector<DrawCommand> m_Commands;
    std::vector<Vertex> m_CapturedVertices;
    bool m_Capturing = false;
    std::uint64_t m_CacheFrame = 0;
    size_t m_CacheBytes = 0;
    size_t m_CacheHits = 0;
    size_t m_CacheMisses = 0;
    PostProcessing m_Post;
    bool m_LinearScene = false;
    std::vector<Vertex> m_Vertices;
    GLuint m_Program = 0;
    GLuint m_VBO = 0;
    GLuint m_VAO = 0;
    GLint m_Viewport = -1;
    GLint m_ColorSpace = -1;
    GLint m_Model = -1;
    GLint m_Tint = -1;
    GLint m_Radiance = -1;
    int m_Width = 1;
    int m_Height = 1;
};
