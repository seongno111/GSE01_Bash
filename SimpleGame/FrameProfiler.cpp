#include "stdafx.h"
#include "FrameProfiler.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

void FrameProfiler::BeginFrame()
{
    if (!m_Started)
    {
        m_WindowStart = Clock::now();
        m_Started = true;
    }
}

void FrameProfiler::EndFrame(std::uint64_t drawCalls)
{
    if (!m_Started)
    {
        return;
    }

    ++m_Frames;
    m_TotalDrawCalls += drawCalls;
    m_MinDrawCalls = (std::min)(m_MinDrawCalls, drawCalls);
    m_MaxDrawCalls = (std::max)(m_MaxDrawCalls, drawCalls);

    const auto now = Clock::now();
    const double seconds = std::chrono::duration<double>(now - m_WindowStart).count();
    if (seconds < 1.0)
    {
        return;
    }

    const double fps = double(m_Frames) / seconds;
    const double average = double(m_TotalDrawCalls) / double(m_Frames);
    std::ostringstream message;
    message << std::fixed << std::setprecision(1) << "[Profile] FPS=" << fps
            << " | DrawCalls/frame: last=" << drawCalls << " avg=" << average
            << " min=" << m_MinDrawCalls << " max=" << m_MaxDrawCalls;
    std::cout << message.str() << '\n' << std::flush;

    // The next interval includes console output time as part of actual FPS.
    m_WindowStart = now;
    m_Frames = 0;
    m_TotalDrawCalls = 0;
    m_MinDrawCalls = (std::numeric_limits<std::uint64_t>::max)();
    m_MaxDrawCalls = 0;
}
