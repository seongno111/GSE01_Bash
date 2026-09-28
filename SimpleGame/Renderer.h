#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <cstdint>
#include <cstddef>
#include <filesystem>
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
        return m_Program && m_VBO && m_VAO && m_Atlas && m_InstanceBuffer && m_InstanceTexture
               && m_MaxInstances > 0 && m_AtlasCapacity > 0;
    }

    void Resize(int width, int height);
    void Begin(Color background);
    // Submit queued instances in painter order. BeginUI flushes automatically
    // before post processing; finish each frame with Flush after drawing UI.
    void Flush();
    void BeginUI();

    std::uint64_t DrawCalls() const
    {
        return m_DrawCalls + m_Post.DrawCalls();
    }

    struct FrameStats
    {
        std::uint64_t batches = 0;
        std::uint64_t instances = 0;
        std::uint64_t triangles = 0;
        std::uint64_t meshBuilds = 0;
        std::uint64_t diskLoads = 0;
        std::uint64_t geometryBytes = 0;
        std::uint64_t instanceBytes = 0;
        std::uint64_t capacityFlushes = 0;
        std::uint64_t atlasRepacks = 0;
        std::uint64_t cacheEvictions = 0;
        std::uint64_t diskRejects = 0;
        std::uint64_t diskWrites = 0;
        std::uint64_t diskWriteSkips = 0;
        std::uint64_t worldDraws = 0;
        std::uint64_t uiDraws = 0;
        double diskReadMs = 0;
        double diskWriteMs = 0;
        double meshBuildMs = 0;
        double geometryUploadMs = 0;
        double batchSubmitMs = 0;
    };

    const FrameStats& Stats() const
    {
        return m_Stats;
    }

    size_t CachedMeshBytes() const
    {
        return m_CacheBytes;
    }

    // Translates a logical screen-space actor, including cached glyph meshes.
    class ScopedTranslation
    {
    public:

        ScopedTranslation(Renderer& renderer, Point offset);
        ~ScopedTranslation();
        ScopedTranslation(const ScopedTranslation&) = delete;
        ScopedTranslation& operator=(const ScopedTranslation&) = delete;

    private:

        Renderer& m_Renderer;
        Point m_Previous;
    };

    // The builder only submits geometry in the supplied coordinates and runs
    // only on both memory and disk cache misses. Nested builders become part
    // of their parent's mesh. GPU geometry is shared by all queued instances.
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
        float padding = 0;
    };

    void VertexAt(Point p, Color c);
    const std::vector<Point>& UnitCircle(int segments);
    void TrimMeshCache();

    struct CachedMesh
    {
        std::vector<Vertex> vertices;
        GLuint first = 0;
    };

    struct Transform
    {
        Point xAxis{1, 0};
        Point yAxis{0, 1};
        Point position{0, 0};
    };

    struct Instance
    {
        float axes[4];
        float placement[4];
        float tint[4];
    };

    struct TriangleInstance
    {
        GLuint first;
        GLuint instance;
    };

    std::shared_ptr<CachedMesh> GetMesh(const std::string& key,
                                        Point position,
                                        float scale,
                                        const std::function<void()>& builder);
    void Submit(const CachedMesh& mesh, const Transform& transform, Color tint);
    void DrawPrimitive(const std::string& key,
                       const Transform& transform,
                       Color tint,
                       const std::function<void()>& builder);
    void InitializeMeshCache();
    bool LoadMeshFile(const std::string& key, std::vector<Vertex>& vertices);
    void SaveMeshFile(const std::string& key, const std::vector<Vertex>& vertices);
    void UploadMesh(CachedMesh& mesh);
    void RepackAtlas();
    std::filesystem::path MeshPath(const std::string& key) const;

    struct CacheEntry
    {
        std::shared_ptr<CachedMesh> mesh;
        std::uint64_t lastUsed = 0;
    };

    std::unordered_map<std::string, CacheEntry> m_MeshCache;
    std::unordered_map<int, std::vector<Point>> m_CircleCache;
    std::vector<Instance> m_Instances;
    std::vector<TriangleInstance> m_Triangles;
    std::vector<Vertex> m_CapturedVertices;
    bool m_Capturing = false;
    Point m_Translation{0, 0};
    std::uint64_t m_CacheFrame = 0;
    size_t m_CacheBytes = 0;
    size_t m_DiskBytes = 0;
    size_t m_CacheHits = 0;
    size_t m_CacheMisses = 0;
    std::uint64_t m_DrawCalls = 0;
    FrameStats m_Stats;
    std::filesystem::path m_CacheDirectory;
    bool m_DiskCacheEnabled = false;
    bool m_DiskWarning = false;
    GLuint m_Atlas = 0;
    size_t m_AtlasUsed = 0;
    size_t m_AtlasCapacity = 0;
    int m_AtlasSide = 0;
    GLuint m_InstanceBuffer = 0;
    GLuint m_InstanceTexture = 0;
    size_t m_MaxInstances = 0;
    PostProcessing m_Post;
    bool m_LinearScene = false;
    bool m_DrawingUI = false;
    GLuint m_Program = 0;
    GLuint m_VBO = 0;
    GLuint m_VAO = 0;
    GLint m_Viewport = -1;
    GLint m_ColorSpace = -1;
    int m_Width = 1;
    int m_Height = 1;
};
