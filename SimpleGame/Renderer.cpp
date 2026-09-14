#include "stdafx.h"
#include "Renderer.h"
#include "ShaderProgram.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>

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

struct Renderer::CachedMesh
{
    GLuint vao = 0;
    GLuint vbo = 0;
    GLsizei count = 0;
    size_t bytes = 0;

    ~CachedMesh()
    {
        if (vbo)
        {
            glDeleteBuffers(1, &vbo);
        }
        if (vao)
        {
            glDeleteVertexArrays(1, &vao);
        }
    }
};

Renderer::Renderer(int width, int height)
    : m_Font(std::make_unique<FontCache>())
{
    Resize(width, height);
    m_Program = LoadShaderProgram("SolidRect.vs", "SolidRect.fs");
    if (!m_Program)
    {
        return;
    }
    m_Viewport = glGetUniformLocation(m_Program, "u_Viewport");
    m_ColorSpace = glGetUniformLocation(m_Program, "u_LinearScene");
    m_Model = glGetUniformLocation(m_Program, "u_Model");
    m_Tint = glGetUniformLocation(m_Program, "u_Tint");
    m_Radiance = glGetUniformLocation(m_Program, "u_Radiance");
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    ConfigureVertexArray(m_VAO, m_VBO);
    m_Vertices.reserve(200000);
    m_Commands.reserve(2048);
    if (!m_Post.Initialize(width, height))
    {
        std::cerr << "Post processing unavailable. Using direct rendering.\n";
    }
}

void Renderer::ConfigureVertexArray(GLuint vao, GLuint vbo)
{
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,
                          4,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<const void*>(sizeof(float) * 2));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,
                          1,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<const void*>(sizeof(float) * 6));
    glBindVertexArray(0);
}

Renderer::~Renderer()
{
    // All GL resources must be released while the renderer's context is alive.
    m_Commands.clear();
    m_MeshCache.clear();
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
    m_Commands.clear();
    m_Vertices.clear();
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
    if (m_Commands.empty() || !IsInitialized())
    {
        return;
    }
    glUseProgram(m_Program);
    glUniform2f(m_Viewport, float(m_Width), float(m_Height));
    glUniform1i(m_ColorSpace, m_LinearScene ? 1 : 0);
    if (!m_Vertices.empty())
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER,
                     m_Vertices.size() * sizeof(Vertex),
                     m_Vertices.data(),
                     GL_STREAM_DRAW);
    }

    // Keep cached and dynamic submissions interleaved: alpha blending depends
    // on painter order, so sorting commands by mesh would change the image.
    for (const DrawCommand& command : m_Commands)
    {
        glBindVertexArray(command.mesh ? command.mesh->vao : m_VAO);
        glUniform4f(m_Model, command.position.x, command.position.y, command.scale, command.scale);
        glUniform4f(m_Tint, command.tint.r, command.tint.g, command.tint.b, command.tint.a);
        glUniform1f(m_Radiance, command.tint.intensity);
        glDrawArrays(GL_TRIANGLES, command.first, command.count);
    }
    glBindVertexArray(0);
    m_Commands.clear();
    m_Vertices.clear();
}

void Renderer::BeginUI()
{
    Flush();
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

    std::shared_ptr<CachedMesh> mesh;
    const auto found = m_MeshCache.find(key);
    if (found != m_MeshCache.end())
    {
        mesh = found->second.mesh;
        found->second.lastUsed = m_CacheFrame;
        ++m_CacheHits;
    }
    else
    {
        ++m_CacheMisses;
        m_CapturedVertices.clear();
        m_Capturing = true;
        try
        {
            builder();
        }
        catch (...)
        {
            m_Capturing = false;
            m_CapturedVertices.clear();
            throw;
        }
        m_Capturing = false;

        mesh = std::make_shared<CachedMesh>();
        mesh->count = static_cast<GLsizei>(m_CapturedVertices.size());
        mesh->bytes = m_CapturedVertices.size() * sizeof(Vertex);
        if (mesh->count > 0)
        {
            glGenVertexArrays(1, &mesh->vao);
            glGenBuffers(1, &mesh->vbo);
            if (!mesh->vao || !mesh->vbo)
            {
                // Retain the current frame's image if a cache buffer cannot
                // be created; these vertices use the ordinary dynamic batch.
                for (const Vertex& vertex : m_CapturedVertices)
                {
                    Color color(vertex.r * tint.r,
                                vertex.g * tint.g,
                                vertex.b * tint.b,
                                vertex.a * tint.a);
                    color.intensity = vertex.intensity * tint.intensity;
                    VertexAt({vertex.x, vertex.y}, color);
                }
                m_CapturedVertices.clear();
                return;
            }
            for (Vertex& vertex : m_CapturedVertices)
            {
                vertex.x = (vertex.x - position.x) / scale;
                vertex.y = (vertex.y - position.y) / scale;
            }
            ConfigureVertexArray(mesh->vao, mesh->vbo);
            glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
            glBufferData(GL_ARRAY_BUFFER, mesh->bytes, m_CapturedVertices.data(), GL_STATIC_DRAW);
        }
        m_CapturedVertices.clear();
        m_MeshCache.emplace(key, CacheEntry{mesh, m_CacheFrame});
        m_CacheBytes += mesh->bytes;
    }

    if (mesh->count > 0)
    {
        DrawCommand command;
        command.mesh = mesh;
        command.count = mesh->count;
        command.position = position;
        command.scale = scale;
        command.tint = tint;
        m_Commands.push_back(command);
    }
}

void Renderer::InvalidateMesh(const std::string& key)
{
    const auto found = m_MeshCache.find(key);
    if (found != m_MeshCache.end())
    {
        m_CacheBytes -= found->second.mesh->bytes;
        m_MeshCache.erase(found);
    }
    // Commands already queued retain ownership until Flush finishes.
}

void Renderer::TrimMeshCache()
{
    constexpr size_t MaxEntries = 4096;
    constexpr size_t MaxBytes = 64 * 1024 * 1024;
    if (m_MeshCache.size() <= MaxEntries && m_CacheBytes <= MaxBytes)
    {
        return;
    }

    // Evict at frame boundaries, after all previous commands have released
    // their resources. The current frame can temporarily exceed this budget.
    std::vector<std::pair<std::uint64_t, std::string>> oldest;
    oldest.reserve(m_MeshCache.size());
    for (const auto& entry : m_MeshCache)
    {
        oldest.emplace_back(entry.second.lastUsed, entry.first);
    }
    std::sort(oldest.begin(), oldest.end());
    for (const auto& entry : oldest)
    {
        if (m_MeshCache.size() <= MaxEntries && m_CacheBytes <= MaxBytes)
        {
            break;
        }
        InvalidateMesh(entry.second);
    }
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
    if (m_Commands.empty() || m_Commands.back().mesh)
    {
        DrawCommand command;
        command.first = static_cast<GLint>(m_Vertices.size());
        m_Commands.push_back(command);
    }
    m_Vertices.push_back({p.x, p.y, c.r, c.g, c.b, c.a, c.intensity});
    ++m_Commands.back().count;
}

void Renderer::Triangle(Point a, Point b, Point c, Color color)
{
    VertexAt(a, color);
    VertexAt(b, color);
    VertexAt(c, color);
}

void Renderer::Quad(Point a, Point b, Point c, Point d, Color color)
{
    Triangle(a, b, c, color);
    Triangle(a, c, d, color);
}

void Renderer::Rect(float x, float y, float width, float height, Color color)
{
    Quad({x, y}, {x + width, y}, {x + width, y + height}, {x, y + height}, color);
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
    Quad({a.x + ox, a.y + oy},
         {b.x + ox, b.y + oy},
         {b.x - ox, b.y - oy},
         {a.x - ox, a.y - oy},
         color);
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
