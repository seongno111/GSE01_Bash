#pragma once
#include "Dependencies/glew.h"

struct PostProcessSettings {
    bool enabled = true;
    bool bloom = true;
    bool vignette = true;
    bool edgeBlur = true;
    float exposure = 1.15f;
    float bloomThreshold = 1.0f; // Linear HDR peak-channel threshold.
    float bloomKnee = 0.5f;
    float bloomStrength = 0.30f;
    float bloomRadius = 2.4f; // Half-resolution pixels at 800px window height.
    float vignetteStrength = 0.52f;
    float vignetteStart = 0.30f;
    float edgeBlurStrength = 0.85f;
    float edgeBlurStart = 0.43f;
    float edgeBlurRadius = 3.0f; // Half-resolution pixels at 800px window height.
};

class PostProcessing {
public:
    PostProcessing() = default;
    ~PostProcessing();
    PostProcessing(const PostProcessing&) = delete;
    PostProcessing& operator=(const PostProcessing&) = delete;
    bool Initialize(int width, int height);
    void Resize(int width, int height);
    bool BeginScene(); // False: caller draws directly to the default framebuffer.
    void Composite();
    bool Available() const { return m_Ready; }
    PostProcessSettings settings;
private:
    struct Target { GLuint fbo = 0, texture = 0; };
    bool CreateTarget(Target& target, int width, int height);
    void DestroyTargets();
    void Draw(GLuint program, GLuint source, const Target& target, int width, int height);
    GLuint Blur(GLuint source, Target (&targets)[2], int rounds, float radius);
    Target m_Scene, m_Bloom[2], m_Blurred[2];
    GLuint m_ExtractProgram = 0, m_BlurProgram = 0, m_CompositeProgram = 0;
    GLuint m_VAO = 0;
    int m_Width = 1, m_Height = 1, m_HalfWidth = 1, m_HalfHeight = 1;
    bool m_Ready = false;
};
