#include "stdafx.h"
#include "PostProcessing.h"
#include "ShaderProgram.h"
#include <algorithm>
#include <iostream>

PostProcessing::~PostProcessing() {
    DestroyTargets();
    if (m_ExtractProgram) glDeleteProgram(m_ExtractProgram);
    if (m_BlurProgram) glDeleteProgram(m_BlurProgram);
    if (m_CompositeProgram) glDeleteProgram(m_CompositeProgram);
    if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
}

bool PostProcessing::Initialize(int width, int height) {
    m_ExtractProgram = LoadShaderProgram("Fullscreen.vs", "BloomExtract.fs");
    m_BlurProgram = LoadShaderProgram("Fullscreen.vs", "GaussianBlur.fs");
    m_CompositeProgram = LoadShaderProgram("Fullscreen.vs", "PostComposite.fs");
    glGenVertexArrays(1, &m_VAO);
    Resize(width, height);
    return m_Ready;
}

bool PostProcessing::CreateTarget(Target& target, int width, int height) {
    glGenTextures(1, &target.texture);
    glBindTexture(GL_TEXTURE_2D, target.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &target.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.texture, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    return target.fbo && target.texture
        && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

void PostProcessing::DestroyTargets() {
    Target* targets[] = {&m_Scene, &m_Bloom[0], &m_Bloom[1], &m_Blurred[0], &m_Blurred[1]};
    for (Target* target : targets) {
        if (target->fbo) glDeleteFramebuffers(1, &target->fbo);
        if (target->texture) glDeleteTextures(1, &target->texture);
        *target = {};
    }
    m_Ready = false;
}

void PostProcessing::Resize(int width, int height) {
    width = (std::max)(1, width); height = (std::max)(1, height);
    if (m_Ready && width == m_Width && height == m_Height) return;
    m_Width = width; m_Height = height;
    m_HalfWidth = (std::max)(1, (width+1)/2);
    m_HalfHeight = (std::max)(1, (height+1)/2);
    DestroyTargets();
    if (!m_ExtractProgram || !m_BlurProgram || !m_CompositeProgram || !m_VAO) return;
    glActiveTexture(GL_TEXTURE0);
    m_Ready = CreateTarget(m_Scene, width, height)
        && CreateTarget(m_Bloom[0], m_HalfWidth, m_HalfHeight)
        && CreateTarget(m_Bloom[1], m_HalfWidth, m_HalfHeight)
        && CreateTarget(m_Blurred[0], m_HalfWidth, m_HalfHeight)
        && CreateTarget(m_Blurred[1], m_HalfWidth, m_HalfHeight);
    if (!m_Ready) {
        std::cerr << "HDR framebuffer creation failed at " << width << "x" << height
            << ". Falling back to direct rendering.\n";
        DestroyTargets();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool PostProcessing::BeginScene() {
    const bool active = settings.enabled && m_Ready;
    glBindFramebuffer(GL_FRAMEBUFFER, active ? m_Scene.fbo : 0);
    glViewport(0, 0, m_Width, m_Height);
    // Gamma encoding is explicit in the final shader; avoid double conversion.
    glDisable(GL_FRAMEBUFFER_SRGB);
    return active;
}

void PostProcessing::Draw(GLuint program, GLuint source, const Target& target, int width, int height) {
    glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
    glViewport(0, 0, width, height);
    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source);
    glUniform1i(glGetUniformLocation(program, "u_Source"), 0);
    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

GLuint PostProcessing::Blur(GLuint source, Target (&targets)[2], int rounds, float radius) {
    glUseProgram(m_BlurProgram);
    for (int i = 0; i < rounds; ++i) {
        glUniform2f(glGetUniformLocation(m_BlurProgram, "u_Direction"), radius, 0);
        // Start with the second target: source may already be targets[0].
        Draw(m_BlurProgram, source, targets[1], m_HalfWidth, m_HalfHeight);
        glUniform2f(glGetUniformLocation(m_BlurProgram, "u_Direction"), 0, radius);
        Draw(m_BlurProgram, targets[1].texture, targets[0], m_HalfWidth, m_HalfHeight);
        source = targets[0].texture;
    }
    return source;
}

void PostProcessing::Composite() {
    if (!m_Ready) return;
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    const float resolutionScale = float(m_Height)/800.0f;
    GLuint bloomTexture = m_Scene.texture; // Unused when strength is zero.
    if (settings.bloom) {
        glUseProgram(m_ExtractProgram);
        glUniform1f(glGetUniformLocation(m_ExtractProgram, "u_Threshold"), settings.bloomThreshold);
        glUniform1f(glGetUniformLocation(m_ExtractProgram, "u_Knee"), settings.bloomKnee);
        Draw(m_ExtractProgram, m_Scene.texture, m_Bloom[0], m_HalfWidth, m_HalfHeight);
        bloomTexture = Blur(m_Bloom[0].texture, m_Bloom, 3, settings.bloomRadius*resolutionScale);
    }
    GLuint blurredTexture = m_Scene.texture;
    if (settings.edgeBlur) {
        // The initial copy performs a 2x2 downsample. Threshold zero keeps all light.
        glUseProgram(m_ExtractProgram);
        glUniform1f(glGetUniformLocation(m_ExtractProgram, "u_Threshold"), 0);
        glUniform1f(glGetUniformLocation(m_ExtractProgram, "u_Knee"), 0);
        Draw(m_ExtractProgram, m_Scene.texture, m_Blurred[0], m_HalfWidth, m_HalfHeight);
        blurredTexture = Blur(m_Blurred[0].texture, m_Blurred, 2, settings.edgeBlurRadius*resolutionScale);
    }
    glUseProgram(m_CompositeProgram);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomTexture);
    glUniform1i(glGetUniformLocation(m_CompositeProgram, "u_Bloom"), 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, blurredTexture);
    glUniform1i(glGetUniformLocation(m_CompositeProgram, "u_BlurredScene"), 2);
    glUniform1f(glGetUniformLocation(m_CompositeProgram, "u_Exposure"), settings.exposure);
    glUniform1f(glGetUniformLocation(m_CompositeProgram, "u_BloomStrength"), settings.bloom ? settings.bloomStrength : 0);
    glUniform1f(glGetUniformLocation(m_CompositeProgram, "u_VignetteStrength"), settings.vignette ? settings.vignetteStrength : 0);
    glUniform1f(glGetUniformLocation(m_CompositeProgram, "u_VignetteStart"), settings.vignetteStart);
    glUniform1f(glGetUniformLocation(m_CompositeProgram, "u_BlurStrength"), settings.edgeBlur ? settings.edgeBlurStrength : 0);
    glUniform1f(glGetUniformLocation(m_CompositeProgram, "u_BlurStart"), settings.edgeBlurStart);
    Draw(m_CompositeProgram, m_Scene.texture, Target{}, m_Width, m_Height);
    for (int unit = 2; unit >= 0; --unit) {
        glActiveTexture(GL_TEXTURE0+unit);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glBindVertexArray(0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
