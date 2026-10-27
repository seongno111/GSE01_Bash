#include "stdafx.h"
#include "Renderer.h"
#include "ShaderProgram.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>

struct Renderer::ModelState
{
    struct Vertex
    {
        float position[3];
        float normal[3];
        float color[4];
        float emission[4];
        GLuint joints[8];
        float weights[8];
        float surface[3];
    };

    struct Range
    {
        size_t first = 0;
        size_t count = 0;
    };

    struct IndexSet
    {
        GLuint buffer = 0;
        GLsizei count = 0;
    };

    struct Mesh
    {
        std::shared_ptr<const Models::Asset> asset;
        GLuint vao = 0;
        GLuint vertices = 0;
        GLuint staticPoseBuffer = 0;
        GLuint staticPoseTexture = 0;
        std::vector<std::uint32_t> indices;
        std::vector<Range> nodes;
        std::vector<size_t> skinOffsets;
        std::map<std::vector<int>, IndexSet> selections;
        Models::Pose bindPose;
        bool needsNodeMatrices = false;
        bool failed = false;

        ~Mesh()
        {
            for (const auto& selection : selections)
            {
                glDeleteBuffers(1, &selection.second.buffer);
            }
            glDeleteBuffers(1, &vertices);
            glDeleteTextures(1, &staticPoseTexture);
            glDeleteBuffers(1, &staticPoseBuffer);
            glDeleteVertexArrays(1, &vao);
        }
    };

    GLuint program = 0;
    GLuint poseBuffer = 0;
    GLuint poseTexture = 0;
    size_t poseCapacity = 16384;
    GLint maxPoseTexels = 0;
    GLint viewport = -1;
    GLint placement = -1;
    GLint orientation = -1;
    GLint tint = -1;
    GLint linearScene = -1;
    GLint lampPosition = -1;
    GLint lampColor = -1;
    std::map<const Models::Asset*, std::unique_ptr<Mesh>> meshes;
    std::vector<Models::Matrix> matrices;

    ~ModelState()
    {
        meshes.clear();
        glDeleteTextures(1, &poseTexture);
        glDeleteBuffers(1, &poseBuffer);
        glDeleteProgram(program);
    }

    Mesh& Get(const std::shared_ptr<const Models::Asset>& asset, Renderer::FrameStats& stats)
    {
        const auto found = meshes.find(asset.get());
        if (found != meshes.end())
        {
            return *found->second;
        }
        auto mesh = std::make_unique<Mesh>();
        mesh->asset = asset;
        auto* result = mesh.get();
        meshes.emplace(asset.get(), std::move(mesh));
        try
        {
            Models::SamplePose(*asset, -1, 0, result->bindPose);
            result->needsNodeMatrices = std::any_of(asset->nodes.begin(),
                                                    asset->nodes.end(),
                                                    [](const Models::Node& node)
                                                    {
                                                        return node.mesh >= 0 && node.skin < 0;
                                                    });
            size_t palette = result->needsNodeMatrices ? asset->nodes.size() : 0;
            for (const auto& skin : asset->skins)
            {
                result->skinOffsets.push_back(palette);
                palette += skin.joints.size();
            }
            if (palette * 4 > static_cast<size_t>(maxPoseTexels))
            {
                throw std::runtime_error("Model skeleton exceeds GPU pose capacity");
            }
            std::vector<Vertex> vertices;
            for (const auto& node : asset->nodes)
            {
                Range range{result->indices.size(), 0};
                if (node.mesh >= 0)
                {
                    for (const auto& primitive : asset->meshes.at(node.mesh).primitives)
                    {
                        const Models::Material fallback;
                        const auto& material = primitive.material >= 0
                                                   ? asset->materials.at(primitive.material)
                                                   : fallback;
                        if (material.occlusionTexture.texture >= 0
                            || material.baseColorTexture.texture >= 0
                            || material.normalTexture.texture >= 0
                            || material.emissiveTexture.texture >= 0
                            || material.metallicRoughnessTexture.texture >= 0)
                        {
                            throw std::runtime_error(
                                "Textured models require an image upload path");
                        }
                        const size_t first = vertices.size();
                        for (const auto& source : primitive.vertices)
                        {
                            Vertex vertex{};
                            std::copy_n(source.position.begin(), 3, vertex.position);
                            std::copy_n(source.normal.begin(), 3, vertex.normal);
                            for (size_t i = 0; i < 4; ++i)
                            {
                                vertex.color[i] = source.color[i] * material.baseColor[i];
                                vertex.emission[i] = material.emissive[i];
                            }
                            vertex.emission[3] = material.unlit ? 1.0f : 0;
                            vertex.surface[0] = material.alphaMode == 1 ? material.alphaCutoff : -1;
                            vertex.surface[1] = material.roughness;
                            vertex.surface[2] = material.metallic;
                            if (node.skin >= 0)
                            {
                                for (size_t i = 0; i < 8; ++i)
                                {
                                    vertex.joints[i] = static_cast<GLuint>(
                                        result->skinOffsets.at(node.skin) + source.joints[i]);
                                    vertex.weights[i] = source.weights[i];
                                }
                            }
                            else
                            {
                                vertex.joints[0] = static_cast<GLuint>(result->nodes.size());
                                vertex.weights[0] = 1;
                            }
                            vertices.push_back(vertex);
                        }
                        for (auto index : primitive.indices)
                        {
                            result->indices.push_back(static_cast<std::uint32_t>(first + index));
                        }
                    }
                }
                range.count = result->indices.size() - range.first;
                result->nodes.push_back(range);
            }
            if (vertices.empty()
                || result->indices.size()
                       > static_cast<size_t>((std::numeric_limits<GLsizei>::max)()))
            {
                throw std::runtime_error(
                    "Model has no drawable triangles or exceeds the index limit");
            }
            glGenVertexArrays(1, &result->vao);
            glGenBuffers(1, &result->vertices);
            glBindVertexArray(result->vao);
            glBindBuffer(GL_ARRAY_BUFFER, result->vertices);
            glBufferData(GL_ARRAY_BUFFER,
                         vertices.size() * sizeof(Vertex),
                         vertices.data(),
                         GL_STATIC_DRAW);
            const auto attribute =
                [](GLuint index, GLint count, size_t offset, bool integer = false)
            {
                glEnableVertexAttribArray(index);
                if (integer)
                {
                    glVertexAttribIPointer(index,
                                           count,
                                           GL_UNSIGNED_INT,
                                           sizeof(Vertex),
                                           reinterpret_cast<const void*>(offset));
                }
                else
                {
                    glVertexAttribPointer(index,
                                          count,
                                          GL_FLOAT,
                                          GL_FALSE,
                                          sizeof(Vertex),
                                          reinterpret_cast<const void*>(offset));
                }
            };
            attribute(0, 3, offsetof(Vertex, position));
            attribute(1, 3, offsetof(Vertex, normal));
            attribute(2, 4, offsetof(Vertex, color));
            attribute(3, 4, offsetof(Vertex, emission));
            attribute(4, 4, offsetof(Vertex, joints), true);
            attribute(5, 4, offsetof(Vertex, weights));
            attribute(6, 4, offsetof(Vertex, joints) + 4 * sizeof(GLuint), true);
            attribute(7, 4, offsetof(Vertex, weights) + 4 * sizeof(float));
            attribute(8, 3, offsetof(Vertex, surface));
            glBindVertexArray(0);
            if (asset->skins.empty() && asset->animations.empty())
            {
                glGenBuffers(1, &result->staticPoseBuffer);
                glGenTextures(1, &result->staticPoseTexture);
                glBindBuffer(GL_TEXTURE_BUFFER, result->staticPoseBuffer);
                const size_t bytes = result->bindPose.world.size() * sizeof(Models::Matrix);
                glBufferData(GL_TEXTURE_BUFFER,
                             bytes,
                             result->bindPose.world.data(),
                             GL_STATIC_DRAW);
                glActiveTexture(GL_TEXTURE3);
                glBindTexture(GL_TEXTURE_BUFFER, result->staticPoseTexture);
                glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, result->staticPoseBuffer);
                glActiveTexture(GL_TEXTURE0);
                stats.geometryBytes += bytes;
            }
            ++stats.modelUploads;
            stats.geometryBytes += vertices.size() * sizeof(Vertex);
            std::cout << "[ModelGPU] uploaded=" << asset->source.filename().u8string()
                      << " vertices=" << vertices.size() << " indices=" << result->indices.size()
                      << " paletteMatrices=" << palette << '\n';
        }
        catch (const std::exception& error)
        {
            result->failed = true;
            std::cerr << "[ModelGPU] unavailable=" << asset->source.u8string()
                      << " reason=" << error.what() << '\n';
        }
        return *result;
    }
};

void Renderer::InitializeModelRenderer()
{
    auto state = std::make_shared<ModelState>();
    state->program = LoadShaderProgram("Model.vs", "Model.fs");
    if (!state->program)
    {
        return;
    }
    glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &state->maxPoseTexels);
    state->viewport = glGetUniformLocation(state->program, "u_Viewport");
    state->placement = glGetUniformLocation(state->program, "u_Placement");
    state->orientation = glGetUniformLocation(state->program, "u_Orientation");
    state->tint = glGetUniformLocation(state->program, "u_Tint");
    state->linearScene = glGetUniformLocation(state->program, "u_LinearScene");
    state->lampPosition = glGetUniformLocation(state->program, "u_LampPosition");
    state->lampColor = glGetUniformLocation(state->program, "u_LampColor");
    glUseProgram(state->program);
    glUniform1i(glGetUniformLocation(state->program, "u_Pose"), 3);
    glGenBuffers(1, &state->poseBuffer);
    glGenTextures(1, &state->poseTexture);
    glBindBuffer(GL_TEXTURE_BUFFER, state->poseBuffer);
    glBufferData(GL_TEXTURE_BUFFER, state->poseCapacity, nullptr, GL_STREAM_DRAW);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_BUFFER, state->poseTexture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, state->poseBuffer);
    glActiveTexture(GL_TEXTURE0);
    m_Models = std::move(state);
}

bool Renderer::DrawModel(const Models::ModelInstance& model,
                         const Models::Pose* suppliedPose,
                         Point feet,
                         float scale,
                         bool facingLeft,
                         bool character,
                         Color tint,
                         float yaw)
{
    if (!model || !m_Models || m_DrawingUI || m_Capturing || !std::isfinite(scale) || scale <= 0)
    {
        return false;
    }
    auto& state = *m_Models;
    auto& mesh = state.Get(model.asset, m_Stats);
    if (mesh.failed)
    {
        return false;
    }
    const auto& asset = *model.asset;
    const auto& pose = suppliedPose ? *suppliedPose : mesh.bindPose;
    if (pose.world.size() != asset.nodes.size())
    {
        return false;
    }
    const auto& key = model.nodes;
    auto found = mesh.selections.find(key);
    if (found == mesh.selections.end())
    {
        std::vector<std::uint32_t> indices;
        for (int node : key)
        {
            if (node < 0 || static_cast<size_t>(node) >= mesh.nodes.size())
            {
                return false;
            }
            const auto range = mesh.nodes[node];
            indices.insert(indices.end(),
                           mesh.indices.begin() + range.first,
                           mesh.indices.begin() + range.first + range.count);
        }
        ModelState::IndexSet selection;
        selection.count = static_cast<GLsizei>(indices.size());
        glGenBuffers(1, &selection.buffer);
        glBindVertexArray(mesh.vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, selection.buffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     indices.size() * sizeof(std::uint32_t),
                     indices.data(),
                     GL_STATIC_DRAW);
        glBindVertexArray(0);
        m_Stats.geometryBytes += indices.size() * sizeof(std::uint32_t);
        found = mesh.selections.emplace(key, selection).first;
    }
    size_t bytes = 0;
    if (!mesh.staticPoseTexture)
    {
        state.matrices.clear();
        if (mesh.needsNodeMatrices)
        {
            state.matrices.assign(pose.world.begin(), pose.world.end());
        }
        for (const auto& skin : asset.skins)
        {
            for (size_t i = 0; i < skin.joints.size(); ++i)
            {
                state.matrices.push_back(
                    Models::Multiply(pose.world[skin.joints[i]], skin.inverseBind[i]));
            }
        }
        bytes = state.matrices.size() * sizeof(Models::Matrix);
    }
    if (character)
    {
        DrawCachedMesh(
            "model-ground-shadow/v1",
            feet,
            scale,
            [&]()
            {
                Ellipse(feet, 19 * scale, 7 * scale, Color(.008f, .012f, .018f, .48f), 16);
            });
    }
    Flush();
    glActiveTexture(GL_TEXTURE3);
    if (mesh.staticPoseTexture)
    {
        glBindTexture(GL_TEXTURE_BUFFER, mesh.staticPoseTexture);
    }
    else
    {
        while (state.poseCapacity < bytes)
        {
            state.poseCapacity *= 2;
        }
        glBindBuffer(GL_TEXTURE_BUFFER, state.poseBuffer);
        glBufferData(GL_TEXTURE_BUFFER, state.poseCapacity, nullptr, GL_STREAM_DRAW);
        glBufferSubData(GL_TEXTURE_BUFFER, 0, bytes, state.matrices.data());
        glBindTexture(GL_TEXTURE_BUFFER, state.poseTexture);
        ++m_Stats.bufferOrphans;
    }
    glUseProgram(state.program);
    glUniform2f(state.viewport, float(m_Width), float(m_Height));
    glUniform3f(state.placement, feet.x + m_Translation.x, feet.y + m_Translation.y, scale);
    // A proper yaw rotation, not negative scale: winding/normals and equipment handedness stay intact.
    const float angle = character ? (facingLeft ? -.44f : 2.01f) : yaw;
    glUniform4f(state.orientation,
                std::cos(angle),
                -std::sin(angle),
                std::sin(angle),
                std::cos(angle));
    glUniform4f(state.tint, tint.r, tint.g, tint.b, tint.a);
    glUniform1i(state.linearScene, m_LinearScene ? 1 : 0);
    glUniform4f(state.lampPosition, 0, 0, 0, 0);
    glUniform4f(state.lampColor, 0, 0, 0, 0);
    if (!asset.pointLights.empty())
    {
        const auto& light = asset.pointLights.front();
        const auto& transform = pose.world.at(light.node);
        glUniform4f(state.lampPosition, transform[12], transform[13], transform[14], light.range);
        glUniform4f(state.lampColor,
                    light.color[0],
                    light.color[1],
                    light.color[2],
                    light.intensity);
    }
    // Preserve existing actor painter order while resolving a model's own surfaces in depth.
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glClearDepth(1);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, found->second.buffer);
    glDrawElements(GL_TRIANGLES, found->second.count, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    glDisable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);
    ++m_DrawCalls;
    ++m_Stats.modelDraws;
    ++m_Stats.worldDraws;
    ++m_Stats.batches;
    ++m_Stats.instances;
    m_Stats.triangles += found->second.count / 3;
    m_Stats.modelPoseBytes += bytes;
    m_Stats.instanceBytes += bytes;
    return true;
}
