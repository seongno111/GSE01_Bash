#include "stdafx.h"
#include "Renderer.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>

namespace {
    std::string ReadShader(const char* filename) {
        wchar_t executable[32768] = {};
        const DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
        const auto root = length > 0 && length < 32768
            ? std::filesystem::path(executable).parent_path() : std::filesystem::current_path();
        const std::filesystem::path candidates[] = {
            root / "Shaders" / filename,
            root / "../../SimpleGame/Shaders" / filename,
            root / "../SimpleGame/Shaders" / filename,
            std::filesystem::path("Shaders") / filename,
            std::filesystem::path("SimpleGame/Shaders") / filename
        };
        for (const auto& path : candidates) {
            std::ifstream stream(path, std::ios::binary);
            if (stream) return std::string(std::istreambuf_iterator<char>(stream), {});
        }
        std::cerr << "Cannot locate shader: " << filename << "\n";
        return {};
    }

    std::wstring FromUtf8(const std::string& text) {
        if (text.empty()) return {};
        const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (!size) return L"?";
        std::wstring wide(size, L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
            static_cast<int>(text.size()), wide.data(), size);
        return wide;
    }
}

// Rasterize Windows' Korean font once per glyph/size; reuse coverage spans in
// the existing triangle batch. No external font files or compatibility GL calls.
struct Renderer::FontCache {
    struct Span { int x, y, width; float coverage; };
    struct Glyph { int advance = 0, lineHeight = 0; std::vector<Span> spans; };
    HDC dc = CreateCompatibleDC(nullptr);
    HGDIOBJ original = dc ? GetCurrentObject(dc, OBJ_FONT) : nullptr;
    std::map<int, HFONT> fonts;
    std::map<std::pair<int, wchar_t>, Glyph> glyphs;
    bool warned = false;

    ~FontCache() {
        if (dc && original) SelectObject(dc, original);
        for (const auto& font : fonts) DeleteObject(font.second);
        if (dc) DeleteDC(dc);
    }
    const Glyph& Get(wchar_t character, int pixels) {
        const auto key = std::make_pair(pixels, character);
        const auto found = glyphs.find(key);
        if (found != glyphs.end()) return found->second;
        Glyph result;
        result.advance = pixels;
        result.lineHeight = pixels + 4;
        if (dc) {
            auto font = fonts.find(pixels);
            if (font == fonts.end()) {
                const HFONT handle = CreateFontW(-pixels, 0, 0, 0, FW_NORMAL,
                    FALSE, FALSE, FALSE, HANGEUL_CHARSET, OUT_TT_PRECIS,
                    CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                    DEFAULT_PITCH | FF_DONTCARE, L"Malgun Gothic");
                if (handle) font = fonts.emplace(pixels, handle).first;
            }
            if (font != fonts.end()) {
                SelectObject(dc, font->second);
                TEXTMETRICW metrics = {};
                GetTextMetricsW(dc, &metrics);
                result.lineHeight = metrics.tmHeight + 3;
                MAT2 identity = {};
                identity.eM11.value = 1;
                identity.eM22.value = 1;
                GLYPHMETRICS gm = {};
                const DWORD bytes = GetGlyphOutlineW(dc, character, GGO_GRAY8_BITMAP,
                    &gm, 0, nullptr, &identity);
                if (bytes != GDI_ERROR) {
                    result.advance = gm.gmCellIncX;
                    if (bytes) {
                        std::vector<unsigned char> bitmap(bytes);
                        if (GetGlyphOutlineW(dc, character, GGO_GRAY8_BITMAP,
                            &gm, bytes, bitmap.data(), &identity) != GDI_ERROR) {
                            const unsigned stride = (gm.gmBlackBoxX + 3) & ~3u;
                            for (unsigned y = 0; y < gm.gmBlackBoxY; ++y) {
                                unsigned x = 0;
                                while (x < gm.gmBlackBoxX) {
                                    const unsigned start = x;
                                    const unsigned char alpha = bitmap[y*stride+x++];
                                    while (x < gm.gmBlackBoxX && bitmap[y*stride+x] == alpha) ++x;
                                    if (alpha) result.spans.push_back({
                                        gm.gmptGlyphOrigin.x + int(start),
                                        metrics.tmAscent - gm.gmptGlyphOrigin.y + int(y),
                                        int(x-start), float(alpha)/64.0f});
                                }
                            }
                        }
                    }
                    return glyphs.emplace(key, std::move(result)).first->second;
                }
            }
        }
        if (!warned) {
            std::cerr << "Cannot rasterize Korean font (Malgun Gothic).\n";
            warned = true;
        }
        // A visible replacement box for unsupported glyphs or font failures.
        result.spans = {{0,0,pixels,1},{0,pixels-1,pixels,1}};
        for (int y = 1; y < pixels-1; ++y) {
            result.spans.push_back({0,y,1,1});
            result.spans.push_back({pixels-1,y,1,1});
        }
        return glyphs.emplace(key, std::move(result)).first->second;
    }
};

Renderer::Renderer(int width, int height) : m_Font(std::make_unique<FontCache>()) {
    Resize(width, height);
    if (!LoadProgram()) return;
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<const void*>(sizeof(float) * 2));
    glBindVertexArray(0);
    m_Vertices.reserve(200000);
}
Renderer::~Renderer() {
    if (m_VBO) glDeleteBuffers(1, &m_VBO);
    if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
    if (m_Program) glDeleteProgram(m_Program);
}
GLuint Renderer::Compile(GLenum type, const std::string& source) {
    if (source.empty()) return 0;
    const GLuint shader = glCreateShader(type);
    if (!shader) return 0;
    const char* text = source.c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[4096] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Shader compilation failed: " << log << "\n";
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}
bool Renderer::LoadProgram() {
    const GLuint vs = Compile(GL_VERTEX_SHADER, ReadShader("SolidRect.vs"));
    const GLuint fs = Compile(GL_FRAGMENT_SHADER, ReadShader("SolidRect.fs"));
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    m_Program = glCreateProgram();
    if (m_Program) {
        glAttachShader(m_Program, vs);
        glAttachShader(m_Program, fs);
        glLinkProgram(m_Program);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!m_Program) return false;
    GLint success = 0;
    glGetProgramiv(m_Program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[4096] = {};
        glGetProgramInfoLog(m_Program, sizeof(log), nullptr, log);
        std::cerr << "Shader linking failed: " << log << "\n";
        glDeleteProgram(m_Program);
        m_Program = 0;
        return false;
    }
    m_Viewport = glGetUniformLocation(m_Program, "u_Viewport");
    return true;
}
void Renderer::Resize(int width, int height) {
    m_Width = (std::max)(1, width);
    m_Height = (std::max)(1, height);
    glViewport(0, 0, m_Width, m_Height);
}
void Renderer::Begin(Color background) {
    m_Vertices.clear();
    glClearColor(background.r, background.g, background.b, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
void Renderer::Flush() {
    if (m_Vertices.empty() || !IsInitialized()) return;
    glUseProgram(m_Program);
    glUniform2f(m_Viewport, float(m_Width), float(m_Height));
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, m_Vertices.size() * sizeof(Vertex),
        m_Vertices.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_Vertices.size()));
    glBindVertexArray(0);
    m_Vertices.clear();
}
void Renderer::VertexAt(Point p, Color c) {
    m_Vertices.push_back({p.x, p.y, c.r, c.g, c.b, c.a});
}
void Renderer::Triangle(Point a, Point b, Point c, Color color) {
    VertexAt(a, color); VertexAt(b, color); VertexAt(c, color);
}
void Renderer::Quad(Point a, Point b, Point c, Point d, Color color) {
    Triangle(a, b, c, color); Triangle(a, c, d, color);
}
void Renderer::Rect(float x, float y, float width, float height, Color color) {
    Quad({x,y}, {x+width,y}, {x+width,y+height}, {x,y+height}, color);
}
void Renderer::Line(Point a, Point b, float width, Color color) {
    const float dx = b.x-a.x, dy = b.y-a.y;
    const float length = std::sqrt(dx*dx+dy*dy);
    if (length < 0.001f) return;
    const float ox = -dy/length*width*0.5f, oy = dx/length*width*0.5f;
    Quad({a.x+ox,a.y+oy}, {b.x+ox,b.y+oy},
        {b.x-ox,b.y-oy}, {a.x-ox,a.y-oy}, color);
}
void Renderer::Ellipse(Point p, float rx, float ry, Color color, int segments) {
    for (int i = 0; i < segments; ++i) {
        const float a = 6.2831853f*i/segments, b = 6.2831853f*(i+1)/segments;
        Triangle(p, {p.x+std::cos(a)*rx,p.y+std::sin(a)*ry},
            {p.x+std::cos(b)*rx,p.y+std::sin(b)*ry}, color);
    }
}
void Renderer::Glow(Point p, float rx, float ry, Color color) {
    const Color edge(color.r, color.g, color.b, 0);
    for (int i = 0; i < 32; ++i) {
        const float a = 6.2831853f*i/32, b = 6.2831853f*(i+1)/32;
        VertexAt(p, color);
        VertexAt({p.x+std::cos(a)*rx,p.y+std::sin(a)*ry}, edge);
        VertexAt({p.x+std::cos(b)*rx,p.y+std::sin(b)*ry}, edge);
    }
}
float Renderer::TextWidth(const std::string& text, float scale) {
    const int pixels = (std::max)(8, int(std::round(12*scale)));
    float width = 0, line = 0;
    for (wchar_t character : FromUtf8(text)) {
        if (character == L'\n') { width = (std::max)(width,line); line = 0; }
        else if (character != L'\r') line += m_Font->Get(character,pixels).advance;
    }
    return (std::max)(width,line);
}
void Renderer::Text(float x, float y, const std::string& text, Color color, float scale) {
    const int pixels = (std::max)(8, int(std::round(12*scale)));
    const float start = std::round(x);
    x = start; y = std::round(y);
    const int lineHeight = m_Font->Get(L' ',pixels).lineHeight;
    for (wchar_t character : FromUtf8(text)) {
        if (character == L'\n') { x = start; y += lineHeight; continue; }
        if (character == L'\r') continue;
        const auto& glyph = m_Font->Get(character,pixels);
        for (const auto& span : glyph.spans) {
            Color ink = color;
            ink.a *= span.coverage;
            Rect(x+span.x,y+span.y,float(span.width),1,ink);
        }
        x += glyph.advance;
    }
}
