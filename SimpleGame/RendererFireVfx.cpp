#include "stdafx.h"
#include "Renderer.h"
#include "ShaderProgram.h"
#include <algorithm>
#include <iostream>

struct Renderer::FireVfxState
{
    GLuint program = 0;
    GLuint vao = 0;
    GLuint buffer = 0;
    GLint viewport = -1;
    GLint linearScene = -1;

    ~FireVfxState()
    {
        glDeleteBuffers(1, &buffer);
        glDeleteVertexArrays(1, &vao);
        glDeleteProgram(program);
    }
};

void Renderer::InitializeFireVfx()
{
    auto state = std::make_shared<FireVfxState>();
    state->program = LoadShaderProgram("FireVfx.vs", "FireVfx.fs");
    if (!state->program)
    {
        return;
    }
    state->viewport = glGetUniformLocation(state->program, "u_Viewport");
    state->linearScene = glGetUniformLocation(state->program, "u_LinearScene");
    glGenVertexArrays(1, &state->vao);
    glGenBuffers(1, &state->buffer);
    glBindVertexArray(state->vao);
    glBindBuffer(GL_ARRAY_BUFFER, state->buffer);
    glBufferData(GL_ARRAY_BUFFER,
                 FireVfx::InstanceCapacity * sizeof(FireVfxInstance),
                 nullptr,
                 GL_STREAM_DRAW);
    const auto attribute = [](GLuint index, GLint components, size_t offset)
    {
        glEnableVertexAttribArray(index);
        glVertexAttribPointer(index,
                              components,
                              GL_FLOAT,
                              GL_FALSE,
                              sizeof(FireVfxInstance),
                              reinterpret_cast<const void*>(offset));
        glVertexAttribDivisor(index, 1);
    };
    attribute(0, 2, offsetof(FireVfxInstance, center));
    attribute(1, 2, offsetof(FireVfxInstance, xAxis));
    attribute(2, 2, offsetof(FireVfxInstance, yAxis));
    attribute(3, 4, offsetof(FireVfxInstance, color));
    attribute(4, 4, offsetof(FireVfxInstance, shape));
    glBindVertexArray(0);
    m_FireVfx = std::move(state);
    std::cout << "[VFX] ready particles=" << FireVfx::ParticleCapacity
              << " effects=" << FireVfx::EffectCapacity
              << " maxInstances=" << FireVfx::InstanceCapacity << " drawsPerPass=1\n";
}

void Renderer::DrawFireVfx(const std::vector<FireVfxInstance>& instances, std::uint64_t dropped)
{
    m_Stats.vfxDropped = dropped;
    if (!m_FireVfx || instances.empty() || m_DrawingUI || m_Capturing)
    {
        return;
    }
    const auto& state = *m_FireVfx;
    const size_t count = (std::min)(instances.size(), FireVfx::InstanceCapacity);
    const size_t bytes = count * sizeof(FireVfxInstance);
    Flush();
    glUseProgram(state.program);
    glUniform2f(state.viewport, float(m_Width), float(m_Height));
    glUniform1i(state.linearScene, m_LinearScene ? 1 : 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(state.vao);
    glBindBuffer(GL_ARRAY_BUFFER, state.buffer);
    glBufferData(GL_ARRAY_BUFFER,
                 FireVfx::InstanceCapacity * sizeof(FireVfxInstance),
                 nullptr,
                 GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, instances.data());
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, static_cast<GLsizei>(count));
    glBindVertexArray(0);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    ++m_DrawCalls;
    ++m_Stats.worldDraws;
    ++m_Stats.batches;
    ++m_Stats.bufferOrphans;
    ++m_Stats.vfxDraws;
    m_Stats.vfxInstances += count;
    m_Stats.instances += count;
    m_Stats.triangles += count * 2;
    m_Stats.instanceBytes += bytes;
}
