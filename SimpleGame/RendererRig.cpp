#include "stdafx.h"
#include "Renderer.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float Pi = 3.14159265358979323846f;

    Point Rotate(Point point, float angle)
    {
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        return {point.x * cosine - point.y * sine, point.x * sine + point.y * cosine};
    }

    struct LegPose
    {
        Point hip;
        Point knee;
        Point foot;
    };

    LegPose SolveLeg(Point hip, Point foot, float thighLength, float shinLength)
    {
        const float dx = foot.x - hip.x;
        const float dy = foot.y - hip.y;
        const float requested = std::hypot(dx, dy);
        const float distance = std::clamp(requested,
                                          std::abs(thighLength - shinLength) + 0.001f,
                                          thighLength + shinLength - 0.001f);
        const Point direction =
            requested > 0.00001f ? Point{dx / requested, dy / requested} : Point{0, 1};
        const float along =
            (thighLength * thighLength - shinLength * shinLength + distance * distance)
            / (2 * distance);
        const float bend = std::sqrt((std::max)(0.0f, thighLength * thighLength - along * along));
        // Screen-right is forward before mirroring. Both knees bend forward;
        // the opposite phase, rather than negative scaling, swaps the lead foot.
        return {hip,
                {hip.x + direction.x * along + direction.y * bend,
                 hip.y + direction.y * along - direction.x * bend},
                {hip.x + direction.x * distance, hip.y + direction.y * distance}};
    }
} // namespace

bool Renderer::DrawUnitRig(unsigned sprite,
                           Point feet,
                           float height,
                           const UnitSpritePose& pose,
                           Color tint)
{
    if (!m_SpriteAtlas || !IsInitialized() || m_Capturing || sprite >= m_UnitRigs.size()
        || !m_UnitRigs[sprite].ready || !std::isfinite(height) || height <= 0
        || !std::isfinite(feet.x) || !std::isfinite(feet.y))
    {
        return false;
    }
    const auto& rig = m_UnitRigs[sprite];
    const auto mesh = GetMesh("sprite/unit-quad-v1",
                              {0, 0},
                              1,
                              [this]()
                              {
                                  Rect(0, 0, 1, 1, Color());
                              });
    const float mirror = pose.facingLeft ? -1.0f : 1.0f;
    const float cycle =
        std::isfinite(pose.walkCycle) ? pose.walkCycle - std::floor(pose.walkCycle) : 0;
    const float weight =
        std::isfinite(pose.locomotionWeight) ? std::clamp(pose.locomotionWeight, 0.0f, 1.0f) : 0;
    const float phase = cycle * 2 * Pi;
    const float stride = std::cos(phase) * weight;
    const float twist = std::sin(phase) * weight;
    float attack = 0;
    if ((sprite == 0 || sprite == 9) && std::isfinite(pose.attackProgress)
        && pose.attackProgress >= 0 && pose.attackProgress < 1)
    {
        // Zero value AND velocity at both ends avoids an attack/idle snap.
        const float release = std::sin(Pi * pose.attackProgress);
        attack = release * release;
    }

    const auto partAt = [&rig](RigPartId id) -> const RigPart&
    {
        return rig.parts[static_cast<size_t>(id)];
    };
    const auto submit = [&](RigPartId id, Transform transform)
    {
        transform.xAxis.x *= height * mirror;
        transform.xAxis.y *= height;
        transform.yAxis.x *= height * mirror;
        transform.yAxis.y *= height;
        transform.position.x = feet.x + transform.position.x * height * mirror;
        transform.position.y = feet.y + transform.position.y * height;
        Submit(*mesh, transform, tint, &partAt(id).region);
    };
    const auto anchored = [&](RigPartId id, Point pivot, float angle, float widthScale, float shear)
    {
        const auto& part = partAt(id);
        const float h = part.region.heightScale;
        const float w = h * part.region.aspect * widthScale;
        Transform transform;
        transform.xAxis = Rotate({w, 0}, angle);
        transform.yAxis = Rotate({shear * h, h}, angle);
        transform.position = {
            pivot.x - transform.xAxis.x * part.start.x - transform.yAxis.x * part.start.y,
            pivot.y - transform.xAxis.y * part.start.x - transform.yAxis.y * part.start.y};
        return transform;
    };
    const auto pointOn = [](const Transform& transform, Point point)
    {
        return Point{
            transform.position.x + transform.xAxis.x * point.x + transform.yAxis.x * point.y,
            transform.position.y + transform.xAxis.y * point.x + transform.yAxis.y * point.y};
    };
    const auto bone = [&](RigPartId id, Point start, Point end)
    {
        const auto& part = partAt(id);
        // Convert normalized texture joints to aspect-correct source space, then
        // map that bone to the target bone with a similarity transform. Width is
        // preserved instead of stretching the leg independently along each axis.
        const float dx = (part.end.x - part.start.x) * part.region.aspect;
        const float dy = part.end.y - part.start.y;
        const float tx = end.x - start.x;
        const float ty = end.y - start.y;
        const float lengthSquared = dx * dx + dy * dy;
        if (lengthSquared <= 0.000001f)
        {
            return;
        }
        const float a = (tx * dx + ty * dy) / lengthSquared;
        const float b = (ty * dx - tx * dy) / lengthSquared;
        Transform transform;
        transform.xAxis = {a * part.region.aspect, b * part.region.aspect};
        transform.yAxis = {-b, a};
        transform.position = {
            start.x - transform.xAxis.x * part.start.x - transform.yAxis.x * part.start.y,
            start.y - transform.xAxis.y * part.start.x - transform.yAxis.y * part.start.y};
        submit(id, transform);
    };

    // No whole-character bob. A small weight shift affects the pelvis/chest;
    // IK keeps the supporting sole on the ground while the other foot swings.
    const float hipX = 0.012f * stride;
    const float hipY = -0.472f - 0.006f * (1 - std::cos(phase * 2)) * weight;
    const float pelvisAngle = 0.035f * stride;
    const Transform pelvis =
        anchored(RigPartId::Pelvis, {hipX, hipY}, pelvisAngle, 1 + 0.025f * twist, 0);
    const Transform chest = anchored(RigPartId::Chest,
                                     {hipX - 0.012f * stride + 0.018f * attack, hipY - 0.072f},
                                     -0.045f * stride + 0.10f * attack,
                                     1 - 0.025f * twist,
                                     0.025f * stride);

    // The near shoulder recedes when the near leg advances. Chest roll, subtle
    // perspective shear, and opposing shoulder height create upper-body motion.
    const auto& chestPart = partAt(RigPartId::Chest);
    const float chestFraction = chestPart.region.heightScale / 0.60f;
    Point farShoulder = pointOn(chest, {0.76f, 0.40f / chestFraction});
    Point nearShoulder = pointOn(chest, {0.22f, 0.46f / chestFraction});
    farShoulder.x += 0.015f * stride;
    farShoulder.y -= 0.008f * stride;
    nearShoulder.x -= 0.015f * stride;
    nearShoulder.y += 0.008f * stride;
    submit(RigPartId::FarArm,
           anchored(RigPartId::FarArm, farShoulder, -0.30f * stride + 0.25f * attack, 1, 0));

    for (unsigned side = 0; side < 2; ++side)
    {
        const bool near = side == 1;
        const float legPhase = phase + (near ? 0 : Pi);
        const Point hipOffset = Rotate({near ? -0.045f : 0.045f, 0.032f}, pelvisAngle);
        const Point hip{hipX + hipOffset.x, hipY + hipOffset.y};
        const float swing = (std::max)(0.0f, std::sin(legPhase));
        const Point foot{(near ? -0.035f : 0.035f) + 0.16f * std::cos(legPhase) * weight,
                         -0.09f * swing * swing * weight};
        const RigPartId thigh = near ? RigPartId::NearThigh : RigPartId::FarThigh;
        const RigPartId shin = near ? RigPartId::NearShin : RigPartId::FarShin;
        const LegPose leg = SolveLeg(hip, foot, partAt(thigh).boneLength, partAt(shin).boneLength);
        bone(thigh, leg.hip, leg.knee);
        bone(shin, leg.knee, leg.foot);
    }

    submit(RigPartId::Pelvis, pelvis);
    submit(RigPartId::Chest, chest);
    submit(RigPartId::NearArm,
           anchored(RigPartId::NearArm, nearShoulder, 0.32f * stride - 1.0f * attack, 1, 0));
    return true;
}
