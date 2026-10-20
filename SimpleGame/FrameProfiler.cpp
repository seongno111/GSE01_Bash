#include "stdafx.h"
#include "FrameProfiler.h"
#include "Renderer.h"
#include "Prototype.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <locale>
#include "Dependencies/wglew.h"

namespace
{
    constexpr const char* CsvHeader =
        "frame,elapsed_s,frame_interval_ms,update_ms,updates,render_cpu_ms,swap_ms,"
        "draw_calls,world_draws,ui_draws,post_draws,batches,logical_instances,triangles,"
        "cache_entries,cache_bytes,cache_hits,cache_misses,mesh_builds,disk_loads,disk_writes,"
        "disk_rejects,disk_write_skips,geometry_upload_bytes,instance_upload_bytes,"
        "capacity_flushes,atlas_repacks,cache_evictions,disk_read_ms,disk_write_ms,"
        "mesh_build_ms,geometry_upload_cpu_ms,batch_submit_cpu_ms,previous_log_flush_ms,"
        "width,height,post_available,post_enabled,bloom,vignette,edge_blur,exposure,"
        "bloom_threshold,bloom_knee,bloom_strength,bloom_radius,vignette_strength,"
        "vignette_start,edge_blur_strength,edge_blur_start,edge_blur_radius,"
        "zoom,grid,camera_x,camera_y,player_x,player_y,scene_actors,loaded_chunks,"
        "world_visible_actors,ui_visible_actors,world_list_cpu_ms,ui_list_cpu_ms,"
        "gameplay_cpu_ms,npc_cpu_ms,stream_cpu_ms,npc_export_cpu_ms,path_calls,path_nodes,"
        "unaccounted_ms,gpu_world_ms,gpu_post_ms,gpu_ui_ms,gpu_sample_age,buffer_orphans,stream_"
        "capacity_bytes\n";

    double Milliseconds(std::chrono::steady_clock::duration duration)
    {
        return std::chrono::duration<double, std::milli>(duration).count();
    }

    std::string GLText(GLenum name)
    {
        const auto* text = glGetString(name);
        return text ? reinterpret_cast<const char*>(text) : "unavailable";
    }
} // namespace

FrameProfiler::~FrameProfiler()
{
    FlushRows();
    if (m_Summary)
    {
        m_Summary << "Session ended. Completed frames=" << m_FrameNumber << '\n';
    }
}

void FrameProfiler::Initialize(Renderer& renderer)
{
    wchar_t executable[32768] = {};
    const DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
    if (!length || length >= 32768)
    {
        std::cerr << "[Profile] Cannot locate executable; console logging only.\n";
        return;
    }
    SYSTEMTIME time = {};
    GetLocalTime(&time);
    std::ostringstream session;
    session << std::setfill('0') << std::setw(4) << time.wYear << std::setw(2) << time.wMonth
            << std::setw(2) << time.wDay << '-' << std::setw(2) << time.wHour << std::setw(2)
            << time.wMinute << std::setw(2) << time.wSecond << '-' << std::setw(3)
            << time.wMilliseconds << '-' << GetCurrentProcessId();
    m_LogDirectory = std::filesystem::path(executable).parent_path() / "Profiling" / session.str();
    std::error_code error;
    std::filesystem::create_directories(m_LogDirectory, error);
    if (error)
    {
        std::cerr << "[Profile] File logging unavailable: " << error.message() << '\n';
        return;
    }
    m_Rows.imbue(std::locale::classic());
    m_Summary.open(m_LogDirectory / "session.log");
    m_Summary.imbue(std::locale::classic());
    m_Summary
        << "Profiling schema=3; renderer=ordered-triangle-instancing; mesh-cache=v1\n"
        << "Build=" << __DATE__ << ' ' << __TIME__ << '\n'
#ifdef _DEBUG
        << "Configuration=Debug\n"
#else
        << "Configuration=Release\n"
#endif
        << "GL_VENDOR=" << GLText(GL_VENDOR) << '\n'
        << "GL_RENDERER=" << GLText(GL_RENDERER) << '\n'
        << "GL_VERSION=" << GLText(GL_VERSION) << '\n'
        << "GLSL_VERSION=" << GLText(GL_SHADING_LANGUAGE_VERSION) << '\n'
        << "Initial viewport=" << renderer.Width() << 'x' << renderer.Height() << '\n'
        << "GPU timestamps are read asynchronously; -1 means no ready sample.\n"
        << "Timer targets absolute 60Hz deadlines, skipping missed updates; simulation dt still "
           "clamps to 50ms.\n"
        << "CSV: one row/completed frame, buffered 1s, new part after 64MiB.\n"
        << "GPU upload bytes count renderer payload only, not post targets or driver traffic.\n"
        << "logical_instances can include chunks of an oversized mesh; triangles are GL "
           "instances.\n"
        << "Summary percentiles use completed frame intervals in each reporting window.\n";
    if (wglewInit() == GLEW_OK && WGLEW_EXT_swap_control && wglGetSwapIntervalEXT)
    {
        m_Summary << "SwapInterval=" << wglGetSwapIntervalEXT() << '\n';
    }
    else
    {
        m_Summary << "SwapInterval=unavailable\n";
    }
    m_Summary << "GpuTimestampAvailable=" << renderer.GpuTimingAvailable() << '\n';
    m_Summary << "GPU age is in rendered frames; GPU stage values are delayed and not paired with "
                 "this row's CPU work.\n"
              << "GPU timestamps include pipeline idle/submission gaps; not hardware utilization "
                 "percentages.\n"
              << "unaccounted_ms=max(0,interval-update-render-swap), includes pacing, event loop, "
                 "prior logging and scheduling.\n"
              << "npc_export_cpu_ms is part of npc_cpu_ms; frame logging flush is separate.\n";
    m_FileLogging = true;
    OpenCsv();
    if (m_FileLogging && m_Summary)
    {
        std::cout << "[Profile] Capture: " << m_LogDirectory.u8string() << '\n';
    }
    else
    {
        std::cerr
            << "[Profile] Could not open all capture files. Console logging remains active.\n";
    }
}

void FrameProfiler::OpenCsv()
{
    m_Csv.close();
    m_Csv.clear();
    m_Csv.open(m_LogDirectory / ("frames-" + std::to_string(++m_Part) + ".csv"));
    m_Csv.imbue(std::locale::classic());
    if (!m_Csv)
    {
        m_FileLogging = false;
        return;
    }
    m_Csv << CsvHeader;
    m_FileBytes = std::char_traits<char>::length(CsvHeader);
}

void FrameProfiler::FlushRows()
{
    if (!m_FileLogging)
    {
        return;
    }
    const std::string rows = m_Rows.str();
    if (rows.empty())
    {
        return;
    }
    if (m_FileBytes + rows.size() > 64 * 1024 * 1024)
    {
        OpenCsv();
    }
    if (m_FileLogging)
    {
        m_Csv << rows << std::flush;
        m_FileBytes += rows.size();
    }
    if (!m_FileLogging || !m_Csv)
    {
        m_FileLogging = false;
        if (!m_FileWarning)
        {
            std::cerr << "[Profile] CSV write failed; console logging remains active.\n";
            m_FileWarning = true;
        }
    }
    m_Rows.str("");
    m_Rows.clear();
}

void FrameProfiler::BeginFrame()
{
    m_FrameStart = Clock::now();
    m_RenderEnd = m_FrameStart;
    if (!m_Started)
    {
        m_WindowStart = m_FrameStart;
        m_SessionStart = m_FrameStart;
        m_PreviousEnd = m_FrameStart;
        m_Started = true;
    }
}

void FrameProfiler::RenderFinished()
{
    m_RenderEnd = Clock::now();
}

void FrameProfiler::RecordUpdate(double milliseconds, const Prototype& game)
{
    m_UpdateMs += milliseconds;
    const auto& stats = game.UpdatingStats();
    m_GameplayMs += stats.gameplayMs;
    m_NpcMs += stats.npcMs;
    m_StreamMs += stats.streamMs;
    m_NpcExportMs += stats.npcExportMs;
    m_PathCalls += stats.pathCalls;
    m_PathNodes += stats.pathNodes;
    ++m_UpdateCount;
}

void FrameProfiler::EndFrame(Renderer& renderer, const Prototype& game)
{
    if (!m_Started)
    {
        return;
    }
    const auto now = Clock::now();
    const double interval = Milliseconds(now - m_PreviousEnd);
    const double renderMs = Milliseconds(m_RenderEnd - m_FrameStart);
    const double swapMs = Milliseconds(now - m_RenderEnd);
    m_PreviousEnd = now;
    const auto& stats = renderer.Stats();
    const auto& scene = game.Scene().RenderingStats();
    const auto camera = game.CameraPosition();
    const auto player = game.Player().WorldPosition();
    const auto& post = renderer.Post();
    const auto& fx = post.settings;
    const auto drawCalls = renderer.DrawCalls();
    ++m_Frames;
    ++m_FrameNumber;
    m_TotalDrawCalls += drawCalls;
    m_MinDrawCalls = (std::min)(m_MinDrawCalls, drawCalls);
    m_MaxDrawCalls = (std::max)(m_MaxDrawCalls, drawCalls);
    m_UpdateTotal += m_UpdateMs;
    m_RenderTotal += renderMs;
    m_SwapTotal += swapMs;
    const double unaccountedMs = (std::max)(0.0, interval - m_UpdateMs - renderMs - swapMs);
    m_NpcTotal += m_NpcMs;
    m_ExportTotal += m_NpcExportMs;
    m_StreamTotal += m_StreamMs;
    m_IdleTotal += unaccountedMs;
    m_DiskTotal += stats.diskReadMs + stats.diskWriteMs;
    if (stats.gpuWorldMs >= 0)
    {
        ++m_GpuSamples;
        m_GpuWorldTotal += stats.gpuWorldMs;
        m_GpuPostTotal += stats.gpuPostMs;
        m_GpuUiTotal += stats.gpuUiMs;
    }
    m_Builds += stats.meshBuilds;
    m_Loads += stats.diskLoads;
    m_FrameTimes.push_back(interval);

    if (m_FileLogging)
    {
        m_Rows << std::fixed << std::setprecision(4) << m_FrameNumber << ','
               << Milliseconds(now - m_SessionStart) / 1000.0 << ',' << interval << ','
               << m_UpdateMs << ',' << m_UpdateCount << ',' << renderMs << ',' << swapMs << ','
               << drawCalls << ',' << stats.worldDraws << ',' << stats.uiDraws << ','
               << post.DrawCalls() << ',' << stats.batches << ',' << stats.instances << ','
               << stats.triangles << ',' << renderer.CachedMeshCount() << ','
               << renderer.CachedMeshBytes() << ',' << renderer.MeshCacheHits() << ','
               << renderer.MeshCacheMisses() << ',' << stats.meshBuilds << ',' << stats.diskLoads
               << ',' << stats.diskWrites << ',' << stats.diskRejects << ',' << stats.diskWriteSkips
               << ',' << stats.geometryBytes << ',' << stats.instanceBytes << ','
               << stats.capacityFlushes << ',' << stats.atlasRepacks << ',' << stats.cacheEvictions
               << ',' << stats.diskReadMs << ',' << stats.diskWriteMs << ',' << stats.meshBuildMs
               << ',' << stats.geometryUploadMs << ',' << stats.batchSubmitMs << ',' << m_LastLogMs
               << ',' << renderer.Width() << ',' << renderer.Height() << ',' << post.Available()
               << ',' << fx.enabled << ',' << fx.bloom << ',' << fx.vignette << ',' << fx.edgeBlur
               << ',' << fx.exposure << ',' << fx.bloomThreshold << ',' << fx.bloomKnee << ','
               << fx.bloomStrength << ',' << fx.bloomRadius << ',' << fx.vignetteStrength << ','
               << fx.vignetteStart << ',' << fx.edgeBlurStrength << ',' << fx.edgeBlurStart << ','
               << fx.edgeBlurRadius << ',' << game.Zoom() << ',' << game.GridEnabled() << ','
               << camera.x << ',' << camera.y << ',' << player.x << ',' << player.y << ','
               << game.Scene().ActorCount() << ',' << game.LoadedChunkCount() << ','
               << scene.worldActors << ',' << scene.uiActors << ',' << scene.worldListMs << ','
               << scene.uiListMs << ',' << m_GameplayMs << ',' << m_NpcMs << ',' << m_StreamMs
               << ',' << m_NpcExportMs << ',' << m_PathCalls << ',' << m_PathNodes << ','
               << unaccountedMs << ',' << stats.gpuWorldMs << ',' << stats.gpuPostMs << ','
               << stats.gpuUiMs << ',' << stats.gpuSampleAge << ',' << stats.bufferOrphans << ','
               << stats.streamCapacityBytes << '\n';
    }
    m_GameplayMs = 0;
    m_NpcMs = 0;
    m_StreamMs = 0;
    m_NpcExportMs = 0;
    m_PathCalls = 0;
    m_PathNodes = 0;
    m_UpdateMs = 0;
    m_UpdateCount = 0;
    const double seconds = std::chrono::duration<double>(now - m_WindowStart).count();
    if (seconds < 1.0)
    {
        return;
    }

    const auto logStart = Clock::now();
    std::sort(m_FrameTimes.begin(), m_FrameTimes.end());
    const auto percentile = [&](double p)
    {
        const size_t index = static_cast<size_t>(std::ceil(p * m_FrameTimes.size())) - 1;
        return m_FrameTimes[(std::min)(index, m_FrameTimes.size() - 1)];
    };
    std::ostringstream message;
    message.imbue(std::locale::classic());
    message << std::fixed << std::setprecision(1) << "[Profile] FPS=" << double(m_Frames) / seconds
            << " | DrawCalls/frame: last=" << drawCalls
            << " avg=" << double(m_TotalDrawCalls) / m_Frames << " min=" << m_MinDrawCalls
            << " max=" << m_MaxDrawCalls << " | CPU ms avg: update=" << m_UpdateTotal / m_Frames
            << " render=" << m_RenderTotal / m_Frames << " swap=" << m_SwapTotal / m_Frames
            << " | Frame ms: p50=" << percentile(0.50) << " p95=" << percentile(0.95)
            << " p99=" << percentile(0.99) << " max=" << m_FrameTimes.back()
            << " | Mesh/window: built=" << m_Builds << " disk=" << m_Loads
            << " | Last draws: world=" << stats.worldDraws << " ui=" << stats.uiDraws
            << " post=" << post.DrawCalls() << " | CPU detail avg ms: npc=" << m_NpcTotal / m_Frames
            << " export=" << m_ExportTotal / m_Frames << " stream=" << m_StreamTotal / m_Frames
            << " cacheIO=" << m_DiskTotal / m_Frames << " pacing/other=" << m_IdleTotal / m_Frames;
    if (m_GpuSamples)
    {
        message << " | GPU delayed avg ms: world=" << m_GpuWorldTotal / m_GpuSamples
                << " post=" << m_GpuPostTotal / m_GpuSamples
                << " ui=" << m_GpuUiTotal / m_GpuSamples << " samples=" << m_GpuSamples;
    }
    else
    {
        message << (renderer.GpuTimingAvailable() ? " | GPU: pending" : " | GPU: unavailable");
    }
    std::cout << message.str() << '\n' << std::flush;
    if (m_Summary)
    {
        m_Summary << "elapsed_s=" << Milliseconds(now - m_SessionStart) / 1000.0
                  << " frames=" << m_Frames << ' ' << message.str() << '\n'
                  << std::flush;
    }
    FlushRows();
    m_LastLogMs = Milliseconds(Clock::now() - logStart);
    m_WindowStart = now;
    m_Frames = 0;
    m_TotalDrawCalls = 0;
    m_MinDrawCalls = (std::numeric_limits<std::uint64_t>::max)();
    m_MaxDrawCalls = 0;
    m_UpdateTotal = 0;
    m_RenderTotal = 0;
    m_SwapTotal = 0;
    m_NpcTotal = 0;
    m_ExportTotal = 0;
    m_StreamTotal = 0;
    m_IdleTotal = 0;
    m_DiskTotal = 0;
    m_GpuSamples = 0;
    m_GpuWorldTotal = 0;
    m_GpuPostTotal = 0;
    m_GpuUiTotal = 0;
    m_Builds = 0;
    m_Loads = 0;
    m_FrameTimes.clear();
}
