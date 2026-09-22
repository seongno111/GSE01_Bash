#pragma once
#include <chrono>
#include <cstdint>
#include <limits>

// Samples completed display frames using wall time, not the simulation's
// clamped delta time. Logging is throttled to avoid per-frame console I/O.
class FrameProfiler
{
public:

    void BeginFrame();
    void EndFrame(std::uint64_t drawCalls);

private:

    using Clock = std::chrono::steady_clock;
    Clock::time_point m_WindowStart;
    bool m_Started = false;
    std::uint64_t m_Frames = 0;
    std::uint64_t m_TotalDrawCalls = 0;
    std::uint64_t m_MinDrawCalls = (std::numeric_limits<std::uint64_t>::max)();
    std::uint64_t m_MaxDrawCalls = 0;
};
