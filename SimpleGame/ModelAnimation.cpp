#include "stdafx.h"
#include "ModelAsset.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Models
{
    namespace
    {
        Vector Normalize(Vector value)
        {
            float sum = 0;
            for (float component : value)
            {
                sum += component * component;
            }
            if (sum < 1e-12f)
            {
                return {0, 0, 0, 1};
            }
            const float inverse = 1 / std::sqrt(sum);
            for (float& component : value)
            {
                component *= inverse;
            }
            return value;
        }

        Vector Lerp(const Vector& a, const Vector& b, float weight)
        {
            Vector result{};
            for (size_t i = 0; i < 4; ++i)
            {
                result[i] = a[i] + (b[i] - a[i]) * weight;
            }
            return result;
        }

        Vector Sample(const Channel& channel, float time)
        {
            const bool cubic = channel.interpolation == Interpolation::CubicSpline;
            const size_t stride = cubic ? 3 : 1;
            const size_t offset = cubic ? 1 : 0;
            const auto upper = std::upper_bound(channel.times.begin(), channel.times.end(), time);
            if (upper == channel.times.begin())
            {
                return channel.values[offset];
            }
            if (upper == channel.times.end())
            {
                return channel.values[(channel.times.size() - 1) * stride + offset];
            }
            const size_t next = static_cast<size_t>(upper - channel.times.begin());
            const size_t previous = next - 1;
            const auto& a = channel.values[previous * stride + offset];
            const auto& b = channel.values[next * stride + offset];
            if (channel.interpolation == Interpolation::Step)
            {
                return a;
            }
            const float interval = channel.times[next] - channel.times[previous];
            const float t = (time - channel.times[previous]) / interval;
            if (!cubic)
            {
                return channel.path == ChannelPath::Rotation ? Slerp(a, b, t) : Lerp(a, b, t);
            }
            const auto& outTangent = channel.values[previous * 3 + 2];
            const auto& inTangent = channel.values[next * 3];
            const float t2 = t * t;
            const float t3 = t2 * t;
            Vector result{};
            for (size_t i = 0; i < 4; ++i)
            {
                result[i] = (2 * t3 - 3 * t2 + 1) * a[i]
                            + (t3 - 2 * t2 + t) * interval * outTangent[i]
                            + (-2 * t3 + 3 * t2) * b[i] + (t3 - t2) * interval * inTangent[i];
            }
            return channel.path == ChannelPath::Rotation ? Normalize(result) : result;
        }
    } // namespace

    Matrix Identity()
    {
        return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    }

    Matrix Multiply(const Matrix& a, const Matrix& b)
    {
        Matrix result{};
        for (size_t column = 0; column < 4; ++column)
        {
            for (size_t row = 0; row < 4; ++row)
            {
                for (size_t k = 0; k < 4; ++k)
                {
                    result[column * 4 + row] += a[k * 4 + row] * b[column * 4 + k];
                }
            }
        }
        return result;
    }

    Matrix Inverse(const Matrix& matrix)
    {
        // Partial pivoting handles negative/nonuniform scale and avoids assuming an orthogonal TRS.
        double rows[4][8]{};
        for (size_t row = 0; row < 4; ++row)
        {
            for (size_t column = 0; column < 4; ++column)
            {
                rows[row][column] = matrix[column * 4 + row];
            }
            rows[row][row + 4] = 1;
        }
        for (size_t column = 0; column < 4; ++column)
        {
            size_t pivot = column;
            for (size_t row = column + 1; row < 4; ++row)
            {
                if (std::abs(rows[row][column]) > std::abs(rows[pivot][column]))
                {
                    pivot = row;
                }
            }
            if (std::abs(rows[pivot][column]) < 1e-12)
            {
                throw std::runtime_error("Cannot invert a singular model transform");
            }
            for (size_t k = 0; k < 8; ++k)
            {
                std::swap(rows[pivot][k], rows[column][k]);
            }
            const double divisor = rows[column][column];
            for (size_t k = 0; k < 8; ++k)
            {
                rows[column][k] /= divisor;
            }
            for (size_t row = 0; row < 4; ++row)
            {
                if (row == column)
                {
                    continue;
                }
                const double factor = rows[row][column];
                for (size_t k = 0; k < 8; ++k)
                {
                    rows[row][k] -= factor * rows[column][k];
                }
            }
        }
        Matrix result{};
        for (size_t row = 0; row < 4; ++row)
        {
            for (size_t column = 0; column < 4; ++column)
            {
                result[column * 4 + row] = static_cast<float>(rows[row][column + 4]);
            }
        }
        return result;
    }

    Vector Slerp(Vector a, Vector b, float weight)
    {
        a = Normalize(a);
        b = Normalize(b);
        float dot = 0;
        for (size_t i = 0; i < 4; ++i)
        {
            dot += a[i] * b[i];
        }
        if (dot < 0)
        {
            dot = -dot;
            for (float& component : b)
            {
                component = -component;
            }
        }
        dot = std::clamp(dot, 0.0f, 1.0f);
        if (dot > .9995f)
        {
            return Normalize(Lerp(a, b, weight));
        }
        const float angle = std::acos(dot);
        const float denominator = std::sin(angle);
        const float left = std::sin((1 - weight) * angle) / denominator;
        const float right = std::sin(weight * angle) / denominator;
        Vector result{};
        for (size_t i = 0; i < 4; ++i)
        {
            result[i] = a[i] * left + b[i] * right;
        }
        return Normalize(result);
    }

    Matrix Transform::LocalMatrix() const
    {
        if (matrixAuthored)
        {
            return matrix;
        }
        const auto q = Normalize(rotation);
        const float x = q[0];
        const float y = q[1];
        const float z = q[2];
        const float w = q[3];
        Matrix result = Identity();
        result[0] = (1 - 2 * (y * y + z * z)) * scale[0];
        result[1] = 2 * (x * y + z * w) * scale[0];
        result[2] = 2 * (x * z - y * w) * scale[0];
        result[4] = 2 * (x * y - z * w) * scale[1];
        result[5] = (1 - 2 * (x * x + z * z)) * scale[1];
        result[6] = 2 * (y * z + x * w) * scale[1];
        result[8] = 2 * (x * z + y * w) * scale[2];
        result[9] = 2 * (y * z - x * w) * scale[2];
        result[10] = (1 - 2 * (x * x + y * y)) * scale[2];
        std::copy_n(translation.begin(), 3, result.begin() + 12);
        return result;
    }

    void UpdateWorldPose(const Asset& asset, Pose& pose)
    {
        if (pose.local.size() != asset.nodes.size())
        {
            throw std::invalid_argument("Pose belongs to a different skeleton");
        }
        pose.world.resize(asset.nodes.size());
        for (int index : asset.evaluationOrder)
        {
            const int parent = asset.nodes[index].parent;
            const auto local = pose.local[index].LocalMatrix();
            pose.world[index] = parent < 0 ? local : Multiply(pose.world[parent], local);
        }
    }

    void SamplePose(const Asset& asset, int clipIndex, double seconds, Pose& pose)
    {
        pose.local.resize(asset.nodes.size());
        for (size_t i = 0; i < asset.nodes.size(); ++i)
        {
            pose.local[i] = asset.nodes[i].bind;
        }
        if (clipIndex >= 0)
        {
            const auto& clip = asset.animations.at(clipIndex);
            double time = std::isfinite(seconds) ? (std::max)(0.0, seconds) : 0;
            time = clip.loop && clip.duration > 0 ? std::fmod(time, clip.duration)
                                                  : (std::min)(time, double(clip.duration));
            for (const auto& channel : clip.channels)
            {
                const auto value = Sample(channel, static_cast<float>(time));
                auto& target = pose.local[channel.node];
                if (channel.path == ChannelPath::Translation)
                {
                    target.translation = value;
                }
                else if (channel.path == ChannelPath::Rotation)
                {
                    target.rotation = value;
                }
                else
                {
                    target.scale = value;
                }
            }
        }
        UpdateWorldPose(asset, pose);
    }

    void BlendPose(const Asset& asset, const Pose& from, const Pose& to, float weight, Pose& result)
    {
        if (from.local.size() != asset.nodes.size() || to.local.size() != asset.nodes.size())
        {
            throw std::invalid_argument("Cannot blend different skeletons");
        }
        weight = std::clamp(weight, 0.0f, 1.0f);
        result.local.resize(asset.nodes.size());
        for (size_t i = 0; i < asset.nodes.size(); ++i)
        {
            const Transform a = from.local[i];
            const Transform b = to.local[i];
            auto& target = result.local[i];
            target = b;
            target.translation = Lerp(a.translation, b.translation, weight);
            target.rotation = Slerp(a.rotation, b.rotation, weight);
            target.scale = Lerp(a.scale, b.scale, weight);
        }
        UpdateWorldPose(asset, result);
    }

    std::vector<Matrix> SkinMatrices(const Asset& asset, const Pose& pose, int meshNode)
    {
        std::vector<Matrix> result;
        SkinMatrices(asset, pose, meshNode, result);
        return result;
    }

    void
    SkinMatrices(const Asset& asset, const Pose& pose, int meshNode, std::vector<Matrix>& output)
    {
        if (pose.world.size() != asset.nodes.size())
        {
            throw std::invalid_argument("Skinning requires a world pose");
        }
        const auto& node = asset.nodes.at(meshNode);
        if (node.skin < 0)
        {
            output.clear();
            return;
        }
        const auto& skin = asset.skins.at(node.skin);
        const auto meshInverse = Inverse(pose.world.at(meshNode));
        output.resize(skin.joints.size());
        for (size_t i = 0; i < skin.joints.size(); ++i)
        {
            output[i] =
                Multiply(Multiply(meshInverse, pose.world.at(skin.joints[i])), skin.inverseBind[i]);
        }
    }

    AnimationPlayer::AnimationPlayer(std::shared_ptr<const Asset> asset)
        : m_Asset(std::move(asset))
    {
        if (!m_Asset)
        {
            throw std::invalid_argument("Animation player requires a model asset");
        }
        SamplePose(*m_Asset, -1, 0, m_Pose);
    }

    bool AnimationPlayer::Play(const std::string& name,
                               float fadeSeconds,
                               bool preservePhase,
                               bool restart)
    {
        const auto found = m_Asset->animationByName.find(name);
        if (found == m_Asset->animationByName.end())
        {
            return false;
        }
        if (found->second == m_Clip && !restart)
        {
            return true;
        }
        double phase = 0;
        if (preservePhase && m_Clip >= 0)
        {
            const float duration = m_Asset->animations[m_Clip].duration;
            phase = duration > 0 ? std::fmod(m_Time, duration) / duration : 0;
        }
        m_From = m_Pose;
        m_Clip = found->second;
        m_Time = phase * m_Asset->animations[m_Clip].duration;
        m_Fade = std::isfinite(fadeSeconds) ? (std::max)(0.0f, fadeSeconds) : 0;
        m_FadeElapsed = 0;
        if (m_Fade == 0)
        {
            SamplePose(*m_Asset, m_Clip, m_Time, m_Pose);
        }
        return true;
    }

    void AnimationPlayer::SetRate(float rate)
    {
        m_Rate = std::isfinite(rate) ? std::clamp(rate, 0.0f, 8.0f) : 1;
    }

    void AnimationPlayer::SetTime(double seconds)
    {
        if (m_Clip < 0 || !std::isfinite(seconds))
        {
            return;
        }
        m_Time = (std::max)(0.0, seconds);
        SamplePose(*m_Asset, m_Clip, m_Time, m_Pose);
        if (m_Fade > 0 && m_FadeElapsed < m_Fade)
        {
            BlendPose(*m_Asset, m_From, m_Pose, m_FadeElapsed / m_Fade, m_Pose);
        }
    }

    void AnimationPlayer::Advance(float seconds, std::vector<AnimationEvent>& events)
    {
        if (m_Clip < 0 || !std::isfinite(seconds) || seconds <= 0)
        {
            return;
        }
        const auto& clip = m_Asset->animations[m_Clip];
        const double previous = m_Time;
        m_Time += seconds * m_Rate;
        if (!clip.loop)
        {
            m_Time = (std::min)(m_Time, double(clip.duration));
        }
        // Markers are notifications only. No damage or projectile is spawned here.
        for (const auto& event : clip.events)
        {
            if (clip.loop && clip.duration > 0)
            {
                double occurrence =
                    event.time
                    + (std::floor((previous - event.time) / clip.duration) + 1) * clip.duration;
                // Bound catch-up on long pauses; do not produce an unbounded event queue.
                int emitted = 0;
                while (occurrence <= m_Time && emitted < 64)
                {
                    if (occurrence > previous)
                    {
                        events.push_back(event);
                        ++emitted;
                    }
                    occurrence += clip.duration;
                }
            }
            else if (event.time > previous && event.time <= m_Time)
            {
                events.push_back(event);
            }
        }
        SamplePose(*m_Asset, m_Clip, m_Time, m_Pose);
        m_FadeElapsed += seconds;
        if (m_Fade > 0 && m_FadeElapsed < m_Fade)
        {
            BlendPose(*m_Asset, m_From, m_Pose, m_FadeElapsed / m_Fade, m_Pose);
        }
    }
} // namespace Models
