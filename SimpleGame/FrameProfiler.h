#pragma once
#include <chrono>
#include <cstdint>
#include <limits>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

class Renderer;
class Prototype;

// Samples completed display frames using wall time, not the simulation's
// clamped delta time. Logging is throttled to avoid per-frame console I/O.
class FrameProfiler
{
public:

    ~FrameProfiler();
    void Initialize(Renderer& renderer);
    void BeginFrame();
    void RenderFinished();
    void RecordUpdate(double milliseconds);
    void EndFrame(Renderer& renderer, const Prototype& game);

private:

    using Clock = std::chrono::steady_clock;
    Clock::time_point m_WindowStart;
    Clock::time_point m_SessionStart;
    Clock::time_point m_FrameStart;
    Clock::time_point m_RenderEnd;
    Clock::time_point m_PreviousEnd;
    bool m_Started = false;
    std::uint64_t m_Frames = 0;
    std::uint64_t m_TotalDrawCalls = 0;
    std::uint64_t m_MinDrawCalls = (std::numeric_limits<std::uint64_t>::max)();
    std::uint64_t m_MaxDrawCalls = 0;
    std::uint64_t m_FrameNumber = 0;
    double m_UpdateMs = 0;
    unsigned m_UpdateCount = 0;
    double m_UpdateTotal = 0;
    double m_RenderTotal = 0;
    double m_SwapTotal = 0;
    double m_LastLogMs = 0;
    std::uint64_t m_Builds = 0;
    std::uint64_t m_Loads = 0;
    std::vector<double> m_FrameTimes;
    std::filesystem::path m_LogDirectory;
    std::ofstream m_Csv;
    std::ofstream m_Summary;
    std::ostringstream m_Rows;
    size_t m_FileBytes = 0;
    unsigned m_Part = 0;
    bool m_FileLogging = false;
    bool m_FileWarning = false;
    void OpenCsv();
    void FlushRows();
};
