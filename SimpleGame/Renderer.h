#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Dependencies/glew.h"
#include "PostProcessing.h"

struct Point { float x, y; };
struct Color {
    float r, g, b, a;
    float intensity = 1;
    Color(float red = 1, float green = 1, float blue = 1, float alpha = 1)
        : r(red), g(green), b(blue), a(alpha) {}
    Color Shade(float factor) const { return Color(r * factor, g * factor, b * factor, a); }
    // RGB is authored in sRGB; intensity multiplies it after linearization.
    Color Radiance(float value) const { Color result = *this; result.intensity = value; return result; }
};

// Pixel coordinates, top-left origin. Submission order defines painter ordering.
class Renderer {
public:
    Renderer(int width, int height);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    bool IsInitialized() const { return m_Program != 0 && m_VBO != 0 && m_VAO != 0; }
    void Resize(int width, int height);
    void Begin(Color background);
    void Flush();
    void BeginUI();
    PostProcessing& Post() { return m_Post; }
    void Triangle(Point a, Point b, Point c, Color color);
    void Quad(Point a, Point b, Point c, Point d, Color color);
    void Rect(float x, float y, float width, float height, Color color);
    void Line(Point a, Point b, float width, Color color);
    void Ellipse(Point center, float rx, float ry, Color color, int segments = 24);
    void Glow(Point center, float rx, float ry, Color color);
    void Text(float x, float y, const std::string& text, Color color, float scale = 2);
    float TextWidth(const std::string& text, float scale = 2);
    int Width() const { return m_Width; }
    int Height() const { return m_Height; }
private:
    struct FontCache;
    std::unique_ptr<FontCache> m_Font;
    struct Vertex { float x, y, r, g, b, a, intensity; };
    void VertexAt(Point p, Color c);
    PostProcessing m_Post;
    bool m_LinearScene = false;
    std::vector<Vertex> m_Vertices;
    GLuint m_Program = 0, m_VBO = 0, m_VAO = 0;
    GLint m_Viewport = -1, m_ColorSpace = -1;
    int m_Width = 1, m_Height = 1;
};
