#include "stdafx.h"
#include "Renderer.h"
#include "ShaderProgram.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>
#include <chrono>

namespace
{
    float Linear(float value)
    {
        return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
    }

    std::wstring FromUtf8(const std::string& text)
    {
        if (text.empty())
        {
            return {};
        }
        const int size = MultiByteToWideChar(CP_UTF8,
                                             MB_ERR_INVALID_CHARS,
                                             text.data(),
                                             static_cast<int>(text.size()),
                                             nullptr,
                                             0);
        if (!size)
        {
            return L"?";
        }
        std::wstring wide(size, L'\0');
        MultiByteToWideChar(CP_UTF8,
                            MB_ERR_INVALID_CHARS,
                            text.data(),
                            static_cast<int>(text.size()),
                            wide.data(),
                            size);
        return wide;
    }
} // namespace

// Rasterize Windows' Korean font once per glyph/size, then upload each glyph as
// a reusable mesh. No external font files or compatibility GL calls.
struct Renderer::FontCache
{
    struct Span
    {
        int x;
        int y;
        int width;
        float coverage;
    };

    struct Glyph
    {
        int advance = 0;
        int lineHeight = 0;
        std::vector<Span> spans;
    };

    HDC dc = CreateCompatibleDC(nullptr);
    HGDIOBJ original = dc ? GetCurrentObject(dc, OBJ_FONT) : nullptr;
    std::map<int, HFONT> fonts;
    std::map<std::pair<int, wchar_t>, Glyph> glyphs;
    bool warned = false;

    ~FontCache()
    {
        if (dc && original)
        {
            SelectObject(dc, original);
        }
        for (const auto& font : fonts)
        {
            DeleteObject(font.second);
        }
        if (dc)
        {
            DeleteDC(dc);
        }
    }

    const Glyph& Get(wchar_t character, int pixels)
    {
        const auto key = std::make_pair(pixels, character);
        const auto found = glyphs.find(key);
        if (found != glyphs.end())
        {
            return found->second;
        }
        Glyph result;
        result.advance = pixels;
        result.lineHeight = pixels + 4;
        if (dc)
        {
            auto font = fonts.find(pixels);
            if (font == fonts.end())
            {
                const HFONT handle = CreateFontW(-pixels,
                                                 0,
                                                 0,
                                                 0,
                                                 FW_NORMAL,
                                                 FALSE,
                                                 FALSE,
                                                 FALSE,
                                                 HANGEUL_CHARSET,
                                                 OUT_TT_PRECIS,
                                                 CLIP_DEFAULT_PRECIS,
                                                 ANTIALIASED_QUALITY,
                                                 DEFAULT_PITCH | FF_DONTCARE,
                                                 L"Malgun Gothic");
                if (handle)
                {
                    font = fonts.emplace(pixels, handle).first;
                }
            }
            if (font != fonts.end())
            {
                SelectObject(dc, font->second);
                TEXTMETRICW metrics = {};
                GetTextMetricsW(dc, &metrics);
                result.lineHeight = metrics.tmHeight + 3;
                MAT2 identity = {};
                identity.eM11.value = 1;
                identity.eM22.value = 1;
                GLYPHMETRICS gm = {};
                const DWORD bytes =
                    GetGlyphOutlineW(dc, character, GGO_GRAY8_BITMAP, &gm, 0, nullptr, &identity);
                if (bytes != GDI_ERROR)
                {
                    result.advance = gm.gmCellIncX;
                    if (bytes)
                    {
                        std::vector<unsigned char> bitmap(bytes);
                        if (GetGlyphOutlineW(dc,
                                             character,
                                             GGO_GRAY8_BITMAP,
                                             &gm,
                                             bytes,
                                             bitmap.data(),
                                             &identity)
                            != GDI_ERROR)
                        {
                            const unsigned stride = (gm.gmBlackBoxX + 3) & ~3u;
                            for (unsigned y = 0; y < gm.gmBlackBoxY; ++y)
                            {
                                unsigned x = 0;
                                while (x < gm.gmBlackBoxX)
                                {
                                    const unsigned start = x;
                                    const unsigned char alpha = bitmap[y * stride + x++];
                                    while (x < gm.gmBlackBoxX && bitmap[y * stride + x] == alpha)
                                    {
                                        ++x;
                                    }
                                    if (alpha)
                                    {
                                        result.spans.push_back(
                                            {gm.gmptGlyphOrigin.x + int(start),
                                             metrics.tmAscent - gm.gmptGlyphOrigin.y + int(y),
                                             int(x - start),
                                             float(alpha) / 64.0f});
                                    }
                                }
                            }
                        }
                    }
                    return glyphs.emplace(key, std::move(result)).first->second;
                }
            }
        }
        if (!warned)
        {
            std::cerr << "Cannot rasterize Korean font (Malgun Gothic).\n";
            warned = true;
        }
        // A visible replacement box for unsupported glyphs or font failures.
        result.spans = {{0, 0, pixels, 1}, {0, pixels - 1, pixels, 1}};
        for (int y = 1; y < pixels - 1; ++y)
        {
            result.spans.push_back({0, y, 1, 1});
            result.spans.push_back({pixels - 1, y, 1, 1});
        }
        return glyphs.emplace(key, std::move(result)).first->second;
    }
};

Renderer::Renderer(int width, int height)
    : m_Font(std::make_unique<FontCache>())
{
    static_assert(sizeof(Instance) == 12 * sizeof(float), "Three RGBA32F texels per instance");
    static_assert(sizeof(TriangleInstance) == 2 * sizeof(GLuint), "Two integer vertex attributes");
    Resize(width, height);
    m_Program = LoadShaderProgram("SolidRect.vs", "SolidRect.fs");
    if (!m_Program)
    {
        return;
    }
    m_Viewport = glGetUniformLocation(m_Program, "u_Viewport");
    m_ColorSpace = glGetUniformLocation(m_Program, "u_LinearScene");
    glUseProgram(m_Program);
    glUniform1i(glGetUniformLocation(m_Program, "u_Geometry"), 0);
    glUniform1i(glGetUniformLocation(m_Program, "u_Instances"), 1);
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glEnableVertexAttribArray(0);
    glVertexAttribIPointer(0, 2, GL_UNSIGNED_INT, sizeof(TriangleInstance), nullptr);
    glVertexAttribDivisor(0, 1);
    glBindVertexArray(0);
    glGenBuffers(1, &m_InstanceBuffer);
    glGenTextures(1, &m_InstanceTexture);
    GLint maxTexels = 0;
    glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxTexels);
    m_MaxInstances = (std::min)(size_t(maxTexels / 3), size_t(16384));
    m_Instances.reserve(m_MaxInstances);
    m_Triangles.reserve(262144);
    InitializeMeshCache();
    if (!m_Post.Initialize(width, height))
    {
        std::cerr << "Post processing unavailable. Using direct rendering.\n";
    }
}

Renderer::~Renderer()
{
    // All GL resources must be released while the renderer's context is alive.
    m_MeshCache.clear();
    glDeleteTextures(1, &m_Atlas);
    glDeleteTextures(1, &m_InstanceTexture);
    glDeleteBuffers(1, &m_InstanceBuffer);
    if (m_VBO)
    {
        glDeleteBuffers(1, &m_VBO);
    }
    if (m_VAO)
    {
        glDeleteVertexArrays(1, &m_VAO);
    }
    if (m_Program)
    {
        glDeleteProgram(m_Program);
    }
}

void Renderer::Resize(int width, int height)
{
    m_Width = (std::max)(1, width);
    m_Height = (std::max)(1, height);
    m_Post.Resize(m_Width, m_Height);
    glViewport(0, 0, m_Width, m_Height);
}

void Renderer::Begin(Color background)
{
    m_DrawCalls = 0;
    m_DrawingUI = false;
    m_Translation = {0, 0};
    m_Instances.clear();
    m_Triangles.clear();
    m_Stats = {};
    ++m_CacheFrame;
    m_CacheHits = 0;
    m_CacheMisses = 0;
    TrimMeshCache();
    m_LinearScene = m_Post.BeginScene();
    if (m_LinearScene)
    {
        background.r = Linear(background.r);
        background.g = Linear(background.g);
        background.b = Linear(background.b);
    }
    glClearColor(background.r, background.g, background.b, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::Flush()
{
    if (m_Triangles.empty() || !IsInitialized())
    {
        return;
    }
    const auto started = std::chrono::steady_clock::now();
    glUseProgram(m_Program);
    glUniform2f(m_Viewport, float(m_Width), float(m_Height));
    glUniform1i(m_ColorSpace, m_LinearScene ? 1 : 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_Atlas);
    glActiveTexture(GL_TEXTURE1);
    glBindBuffer(GL_TEXTURE_BUFFER, m_InstanceBuffer);
    glBufferData(GL_TEXTURE_BUFFER,
                 m_Instances.size() * sizeof(Instance),
                 m_Instances.data(),
                 GL_STREAM_DRAW);
    glBindTexture(GL_TEXTURE_BUFFER, m_InstanceTexture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, m_InstanceBuffer);
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 m_Triangles.size() * sizeof(TriangleInstance),
                 m_Triangles.data(),
                 GL_STREAM_DRAW);
    // Every instance is one triangle. Both triangle and object order match
    // submission order, even across unrelated meshes and transparent glyphs.
    glDrawArraysInstanced(GL_TRIANGLES, 0, 3, static_cast<GLsizei>(m_Triangles.size()));
    ++m_DrawCalls;
    ++m_Stats.batches;
    m_DrawingUI ? ++m_Stats.uiDraws : ++m_Stats.worldDraws;
    m_Stats.instanceBytes +=
        m_Instances.size() * sizeof(Instance) + m_Triangles.size() * sizeof(TriangleInstance);
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
    m_Instances.clear();
    m_Triangles.clear();
    m_Stats.batchSubmitMs +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count();
}

void Renderer::Submit(const CachedMesh& mesh, const Transform& transform, Color tint)
{
    if (mesh.vertices.empty() || !IsInitialized())
    {
        return;
    }
    constexpr size_t MaxTriangles = 262144;
    size_t first = 0;
    while (first < mesh.vertices.size())
    {
        if (m_Instances.size() >= m_MaxInstances || m_Triangles.size() == MaxTriangles)
        {
            ++m_Stats.capacityFlushes;
            Flush();
        }
        const GLuint instance = static_cast<GLuint>(m_Instances.size());
        m_Instances.push_back(
            {{transform.xAxis.x, transform.xAxis.y, transform.yAxis.x, transform.yAxis.y},
             {transform.position.x + m_Translation.x,
              transform.position.y + m_Translation.y,
              tint.intensity,
              0},
             {tint.r, tint.g, tint.b, tint.a}});
        const size_t count =
            (std::min)((mesh.vertices.size() - first) / 3, MaxTriangles - m_Triangles.size());
        for (size_t i = 0; i < count; ++i)
        {
            m_Triangles.push_back({mesh.first + static_cast<GLuint>(first + i * 3), instance});
        }
        first += count * 3;
        ++m_Stats.instances;
        m_Stats.triangles += count;
    }
}

void Renderer::DrawPrimitive(const std::string& key,
                             const Transform& transform,
                             Color tint,
                             const std::function<void()>& builder)
{
    if (!IsInitialized())
    {
        return;
    }
    const auto mesh = GetMesh(key, {0, 0}, 1, builder);
    Submit(*mesh, transform, tint);
}

void Renderer::BeginUI()
{
    Flush();
    m_DrawingUI = true;
    if (m_LinearScene)
    {
        m_Post.Composite();
    }
    m_LinearScene = false;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_Width, m_Height);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::DrawCachedMesh(const std::string& key,
                              Point position,
                              float scale,
                              const std::function<void()>& builder,
                              Color tint)
{
    if (!IsInitialized() || !builder || !std::isfinite(scale) || scale <= 0)
    {
        return;
    }

    // Composite builders (for example a chapel containing blocks) capture
    // their children into one mesh, without queuing separate draw commands.
    if (m_Capturing)
    {
        const size_t first = m_CapturedVertices.size();
        builder();
        for (size_t i = first; i < m_CapturedVertices.size(); ++i)
        {
            Vertex& vertex = m_CapturedVertices[i];
            vertex.r *= tint.r;
            vertex.g *= tint.g;
            vertex.b *= tint.b;
            vertex.a *= tint.a;
            vertex.intensity *= tint.intensity;
        }
        return;
    }

    const auto mesh = GetMesh(key, position, scale, builder);
    Submit(*mesh, {{scale, 0}, {0, scale}, position}, tint);
}

size_t Renderer::CachedMeshCount() const
{
    return m_MeshCache.size();
}

size_t Renderer::MeshCacheHits() const
{
    return m_CacheHits;
}

size_t Renderer::MeshCacheMisses() const
{
    return m_CacheMisses;
}

void Renderer::VertexAt(Point p, Color c)
{
    if (m_Capturing)
    {
        m_CapturedVertices.push_back({p.x, p.y, c.r, c.g, c.b, c.a, c.intensity});
        return;
    }
    // Only cache builders create vertices. Runtime primitives submit transforms.
    throw std::logic_error("VertexAt must be called inside a mesh builder");
}

Renderer::ScopedTranslation::ScopedTranslation(Renderer& renderer, Point offset)
    : m_Renderer(renderer),
      m_Previous(renderer.m_Translation)
{
    renderer.m_Translation.x += offset.x;
    renderer.m_Translation.y += offset.y;
}

Renderer::ScopedTranslation::~ScopedTranslation()
{
    m_Renderer.m_Translation = m_Previous;
}

void Renderer::Triangle(Point a, Point b, Point c, Color color)
{
    if (m_Capturing)
    {
        VertexAt(a, color);
        VertexAt(b, color);
        VertexAt(c, color);
        return;
    }
    DrawPrimitive("primitive/triangle",
                  {{b.x - a.x, b.y - a.y}, {c.x - a.x, c.y - a.y}, a},
                  color,
                  [&]()
                  {
                      Triangle({0, 0}, {1, 0}, {0, 1}, Color());
                  });
}

void Renderer::Quad(Point a, Point b, Point c, Point d, Color color)
{
    Triangle(a, b, c, color);
    Triangle(a, c, d, color);
}

void Renderer::Rect(float x, float y, float width, float height, Color color)
{
    if (m_Capturing)
    {
        Quad({x, y}, {x + width, y}, {x + width, y + height}, {x, y + height}, color);
        return;
    }
    DrawPrimitive("primitive/quad",
                  {{width, 0}, {0, height}, {x, y}},
                  color,
                  [&]()
                  {
                      Rect(0, 0, 1, 1, Color());
                  });
}

void Renderer::Line(Point a, Point b, float width, Color color)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length < 0.001f)
    {
        return;
    }
    const float ox = -dy / length * width * 0.5f;
    const float oy = dx / length * width * 0.5f;
    if (m_Capturing)
    {
        Quad({a.x + ox, a.y + oy},
             {b.x + ox, b.y + oy},
             {b.x - ox, b.y - oy},
             {a.x - ox, a.y - oy},
             color);
        return;
    }
    DrawPrimitive("primitive/quad",
                  {{dx, dy}, {-2 * ox, -2 * oy}, {a.x + ox, a.y + oy}},
                  color,
                  [&]()
                  {
                      Rect(0, 0, 1, 1, Color());
                  });
}

const std::vector<Point>& Renderer::UnitCircle(int segments)
{
    const auto found = m_CircleCache.find(segments);
    if (found != m_CircleCache.end())
    {
        return found->second;
    }
    std::vector<Point> points;
    points.reserve(segments + 1);
    for (int i = 0; i < segments; ++i)
    {
        const float angle = 6.2831853f * i / segments;
        points.push_back({std::cos(angle), std::sin(angle)});
    }
    points.push_back(points.front());
    return m_CircleCache.emplace(segments, std::move(points)).first->second;
}

void Renderer::Ellipse(Point p, float rx, float ry, Color color, int segments)
{
    if (segments < 3)
    {
        return;
    }
    if (!m_Capturing)
    {
        DrawPrimitive("primitive/ellipse/" + std::to_string(segments),
                      {{rx, 0}, {0, ry}, p},
                      color,
                      [&]()
                      {
                          Ellipse({0, 0}, 1, 1, Color(), segments);
                      });
        return;
    }
    const auto& circle = UnitCircle(segments);
    for (int i = 0; i < segments; ++i)
    {
        Triangle(p,
                 {p.x + circle[i].x * rx, p.y + circle[i].y * ry},
                 {p.x + circle[i + 1].x * rx, p.y + circle[i + 1].y * ry},
                 color);
    }
}

void Renderer::Glow(Point p, float rx, float ry, Color color)
{
    if (!m_Capturing)
    {
        DrawPrimitive("primitive/glow",
                      {{rx, 0}, {0, ry}, p},
                      color,
                      [&]()
                      {
                          Glow({0, 0}, 1, 1, Color());
                      });
        return;
    }
    Color edge = color;
    edge.a = 0;
    const auto& circle = UnitCircle(32);
    for (int i = 0; i < 32; ++i)
    {
        VertexAt(p, color);
        VertexAt({p.x + circle[i].x * rx, p.y + circle[i].y * ry}, edge);
        VertexAt({p.x + circle[i + 1].x * rx, p.y + circle[i + 1].y * ry}, edge);
    }
}

float Renderer::TextWidth(const std::string& text, float scale)
{
    const int pixels = (std::max)(8, int(std::round(12 * scale)));
    float width = 0;
    float line = 0;
    for (wchar_t character : FromUtf8(text))
    {
        if (character == L'\n')
        {
            width = (std::max)(width, line);
            line = 0;
        }
        else if (character != L'\r')
        {
            line += m_Font->Get(character, pixels).advance;
        }
    }
    return (std::max)(width, line);
}

void Renderer::Text(float x, float y, const std::string& text, Color color, float scale)
{
    const int pixels = (std::max)(8, int(std::round(12 * scale)));
    const float start = std::round(x);
    x = start;
    y = std::round(y);
    const int lineHeight = m_Font->Get(L' ', pixels).lineHeight;
    for (wchar_t character : FromUtf8(text))
    {
        if (character == L'\n')
        {
            x = start;
            y += lineHeight;
            continue;
        }
        if (character == L'\r')
        {
            continue;
        }
        const auto& glyph = m_Font->Get(character, pixels);
        if (!glyph.spans.empty())
        {
            const std::string key = "glyph/" + std::to_string(pixels) + "/"
                                    + std::to_string(static_cast<unsigned>(character));
            DrawCachedMesh(
                key,
                {x, y},
                1,
                [&]()
                {
                    for (const auto& span : glyph.spans)
                    {
                        Rect(x + span.x,
                             y + span.y,
                             float(span.width),
                             1,
                             Color(1, 1, 1, span.coverage));
                    }
                },
                color);
        }
        x += glyph.advance;
    }
}
