#include "stdafx.h"
#include "Renderer.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <chrono>

namespace
{
    // Bump when mesh recipes, font rasterization or the vertex format change.
    constexpr std::uint32_t CacheVersion = 1;
    constexpr std::uint32_t Magic = 0x4853454d;
    constexpr size_t DiskBudget = 256 * 1024 * 1024;

    struct CpuTimer
    {
        double& value;
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

        ~CpuTimer()
        {
            value +=
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                    .count();
        }
    };

    std::uint64_t Hash(const void* data, size_t bytes)
    {
        auto p = static_cast<const unsigned char*>(data);
        std::uint64_t hash = 14695981039346656037ull;
        for (size_t i = 0; i < bytes; ++i)
        {
            hash = (hash ^ p[i]) * 1099511628211ull;
        }
        return hash;
    }

    template <class T> void Write(std::ostream& stream, const T& value)
    {
        stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    template <class T> void Read(std::istream& stream, T& value)
    {
        stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    }
} // namespace

void Renderer::InitializeMeshCache()
{
    static_assert(sizeof(Vertex) == 8 * sizeof(float), "Atlas uses two RGBA32F texels per vertex");
    GLint maxSide = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSide);
    m_AtlasSide = (std::min)(2048, maxSide);
    m_AtlasCapacity = size_t(m_AtlasSide) * m_AtlasSide / 2;
    glGenTextures(1, &m_Atlas);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_Atlas);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA32F,
                 m_AtlasSide,
                 m_AtlasSide,
                 0,
                 GL_RGBA,
                 GL_FLOAT,
                 nullptr);
    GLint allocatedWidth = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &allocatedWidth);
    if (!m_Atlas || allocatedWidth != m_AtlasSide)
    {
        std::cerr << "[MeshCache] Cannot allocate geometry atlas.\n";
        glDeleteTextures(1, &m_Atlas);
        m_Atlas = 0;
        m_AtlasCapacity = 0;
        return;
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    wchar_t executable[32768] = {};
    const DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
    if (!length || length >= 32768)
    {
        std::cerr << "[MeshCache] Cannot locate executable; using memory cache only.\n";
        return;
    }
    m_CacheDirectory = std::filesystem::path(executable).parent_path() / "MeshCache"
                       / ("v" + std::to_string(CacheVersion));
    std::error_code error;
    std::filesystem::create_directories(m_CacheDirectory, error);
    m_DiskCacheEnabled = !error;
    if (error)
    {
        std::cerr << "[MeshCache] Cannot create cache directory: " << error.message() << '\n';
        return;
    }
    for (std::filesystem::directory_iterator it(m_CacheDirectory, error), end; !error && it != end;
         it.increment(error))
    {
        if (it->path().extension() == ".mesh" && it->is_regular_file(error))
        {
            const auto bytes = it->file_size(error);
            if (!error)
            {
                m_DiskBytes += bytes;
            }
        }
    }
    if (error)
    {
        m_DiskCacheEnabled = false;
        std::cerr << "[MeshCache] Cannot scan cache: " << error.message() << '\n';
    }
    std::cout << "[MeshCache] " << m_CacheDirectory.u8string()
              << " | atlas=" << m_AtlasCapacity * sizeof(Vertex) / (1024 * 1024)
              << " MiB | disk budget=256 MiB\n";
}

std::filesystem::path Renderer::MeshPath(const std::string& key) const
{
    std::ostringstream name;
    name << std::hex << std::setfill('0') << std::setw(16) << Hash(key.data(), key.size())
         << ".mesh";
    return m_CacheDirectory / name.str();
}

bool Renderer::LoadMeshFile(const std::string& key, std::vector<Vertex>& vertices)
{
    CpuTimer timer{m_Stats.diskReadMs};
    if (!m_DiskCacheEnabled)
    {
        return false;
    }
    std::ifstream file(MeshPath(key), std::ios::binary);
    if (!file)
    {
        return false;
    }
    std::uint32_t magic = 0, version = 0, keyBytes = 0, count = 0;
    std::uint64_t checksum = 0;
    Read(file, magic);
    Read(file, version);
    Read(file, keyBytes);
    Read(file, count);
    Read(file, checksum);
    if (!file || magic != Magic || version != CacheVersion || keyBytes != key.size()
        || keyBytes > 4096 || count > m_AtlasCapacity || count % 3 != 0)
    {
        ++m_Stats.diskRejects;
        return false;
    }
    std::string storedKey(keyBytes, '\0');
    file.read(storedKey.data(), keyBytes);
    if (!file || storedKey != key)
    {
        ++m_Stats.diskRejects;
        return false;
    }
    vertices.resize(count);
    file.read(reinterpret_cast<char*>(vertices.data()), count * sizeof(Vertex));
    const bool finite = std::all_of(vertices.begin(),
                                    vertices.end(),
                                    [](const Vertex& v)
                                    {
                                        return std::isfinite(v.x) && std::isfinite(v.y)
                                               && std::isfinite(v.r) && std::isfinite(v.g)
                                               && std::isfinite(v.b) && std::isfinite(v.a)
                                               && std::isfinite(v.intensity);
                                    });
    if (!file || Hash(vertices.data(), count * sizeof(Vertex)) != checksum || !finite
        || file.peek() != std::char_traits<char>::eof())
    {
        vertices.clear();
        ++m_Stats.diskRejects;
        return false;
    }
    ++m_Stats.diskLoads;
    return true;
}

void Renderer::SaveMeshFile(const std::string& key, const std::vector<Vertex>& vertices)
{
    CpuTimer timer{m_Stats.diskWriteMs};
    if (!m_DiskCacheEnabled || key.size() > 4096)
    {
        return;
    }
    const auto path = MeshPath(key);
    std::error_code error;
    const auto existing = std::filesystem::file_size(path, error);
    const auto oldBytes = error ? 0 : existing;
    const size_t bytes = 24 + key.size() + vertices.size() * sizeof(Vertex);
    if (m_DiskBytes + bytes > DiskBudget + oldBytes)
    {
        ++m_Stats.diskWriteSkips;
        if (!m_DiskWarning)
        {
            std::cerr << "[MeshCache] Disk budget reached; new meshes remain in memory only.\n";
            m_DiskWarning = true;
        }
        return;
    }
    auto temp = path;
    temp += "." + std::to_string(GetCurrentProcessId()) + ".tmp";
    std::ofstream file(temp, std::ios::binary | std::ios::trunc);
    Write(file, Magic);
    Write(file, CacheVersion);
    Write(file, static_cast<std::uint32_t>(key.size()));
    Write(file, static_cast<std::uint32_t>(vertices.size()));
    Write(file, Hash(vertices.data(), vertices.size() * sizeof(Vertex)));
    file.write(key.data(), key.size());
    file.write(reinterpret_cast<const char*>(vertices.data()), vertices.size() * sizeof(Vertex));
    file.close();
    if (!file || !MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING))
    {
        ++m_Stats.diskWriteSkips;
        if (!m_DiskWarning)
        {
            std::cerr << "[MeshCache] Cannot save mesh; continuing with memory cache.\n";
            m_DiskWarning = true;
        }
        std::filesystem::remove(temp, error);
        return;
    }
    m_DiskBytes = m_DiskBytes >= oldBytes ? m_DiskBytes - oldBytes + bytes : bytes;
    ++m_Stats.diskWrites;
}

void Renderer::UploadMesh(CachedMesh& mesh)
{
    CpuTimer timer{m_Stats.geometryUploadMs};
    mesh.first = static_cast<GLuint>(m_AtlasUsed);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_Atlas);
    size_t firstTexel = m_AtlasUsed * 2;
    size_t remaining = mesh.vertices.size() * 2;
    const char* data = reinterpret_cast<const char*>(mesh.vertices.data());
    while (remaining)
    {
        const int x = int(firstTexel % m_AtlasSide);
        const int y = int(firstTexel / m_AtlasSide);
        const size_t count = (std::min)(remaining, size_t(m_AtlasSide - x));
        glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        x,
                        y,
                        static_cast<GLsizei>(count),
                        1,
                        GL_RGBA,
                        GL_FLOAT,
                        data);
        firstTexel += count;
        remaining -= count;
        data += count * 4 * sizeof(float);
    }
    m_AtlasUsed += mesh.vertices.size();
    m_Stats.geometryBytes += mesh.vertices.size() * sizeof(Vertex);
}

void Renderer::RepackAtlas()
{
    // Drain all references before changing offsets or overwriting atlas storage.
    Flush();
    m_AtlasUsed = 0;
    for (auto& entry : m_MeshCache)
    {
        UploadMesh(*entry.second.mesh);
    }
    ++m_Stats.atlasRepacks;
}

std::shared_ptr<Renderer::CachedMesh> Renderer::GetMesh(const std::string& key,
                                                        Point position,
                                                        float scale,
                                                        const std::function<void()>& builder)
{
    const auto found = m_MeshCache.find(key);
    if (found != m_MeshCache.end())
    {
        found->second.lastUsed = m_CacheFrame;
        ++m_CacheHits;
        return found->second.mesh;
    }
    ++m_CacheMisses;
    auto mesh = std::make_shared<CachedMesh>();
    if (!LoadMeshFile(key, mesh->vertices))
    {
        m_CapturedVertices.clear();
        m_Capturing = true;
        try
        {
            CpuTimer timer{m_Stats.meshBuildMs};
            builder();
        }
        catch (...)
        {
            m_Capturing = false;
            m_CapturedVertices.clear();
            throw;
        }
        m_Capturing = false;
        mesh->vertices.assign(m_CapturedVertices.begin(), m_CapturedVertices.end());
        for (Vertex& v : mesh->vertices)
        {
            v.x = (v.x - position.x) / scale;
            v.y = (v.y - position.y) / scale;
        }
        ++m_Stats.meshBuilds;
        if (mesh->vertices.size() > m_AtlasCapacity || mesh->vertices.size() % 3 != 0)
        {
            throw std::length_error("Mesh exceeds atlas capacity or contains incomplete triangles");
        }
        SaveMeshFile(key, mesh->vertices);
    }
    if (m_AtlasUsed + mesh->vertices.size() > m_AtlasCapacity)
    {
        Flush();
        // Make headroom for new streamed chunks instead of compacting on each miss.
        const size_t target =
            (std::min)(m_AtlasCapacity * 3 / 4, m_AtlasCapacity - mesh->vertices.size());
        while (m_CacheBytes / sizeof(Vertex) > target && !m_MeshCache.empty())
        {
            const auto oldest = std::min_element(m_MeshCache.begin(),
                                                 m_MeshCache.end(),
                                                 [](const auto& a, const auto& b)
                                                 {
                                                     return a.second.lastUsed < b.second.lastUsed;
                                                 });
            m_CacheBytes -= oldest->second.mesh->vertices.size() * sizeof(Vertex);
            m_MeshCache.erase(oldest);
            ++m_Stats.cacheEvictions;
        }
        RepackAtlas();
    }
    UploadMesh(*mesh);
    m_CacheBytes += mesh->vertices.size() * sizeof(Vertex);
    m_MeshCache.emplace(key, CacheEntry{mesh, m_CacheFrame});
    return mesh;
}

void Renderer::InvalidateMesh(const std::string& key)
{
    const auto found = m_MeshCache.find(key);
    if (found != m_MeshCache.end())
    {
        m_CacheBytes -= found->second.mesh->vertices.size() * sizeof(Vertex);
        m_MeshCache.erase(found);
    }
    if (m_DiskCacheEnabled)
    {
        const auto path = MeshPath(key);
        std::error_code error;
        const auto bytes = std::filesystem::file_size(path, error);
        if (!error && std::filesystem::remove(path, error))
        {
            m_DiskBytes -= (std::min)(m_DiskBytes, size_t(bytes));
        }
    }
    // Queued triangles still refer to immutable atlas storage until the next repack.
}

void Renderer::TrimMeshCache()
{
    constexpr size_t MaxEntries = 4096;
    if (m_MeshCache.size() <= MaxEntries)
    {
        return;
    }
    std::vector<std::pair<std::uint64_t, std::string>> oldest;
    for (const auto& entry : m_MeshCache)
    {
        oldest.emplace_back(entry.second.lastUsed, entry.first);
    }
    std::sort(oldest.begin(), oldest.end());
    const size_t removeCount = m_MeshCache.size() - MaxEntries * 3 / 4;
    for (size_t i = 0; i < removeCount; ++i)
    {
        const auto found = m_MeshCache.find(oldest[i].second);
        m_CacheBytes -= found->second.mesh->vertices.size() * sizeof(Vertex);
        m_MeshCache.erase(found);
        ++m_Stats.cacheEvictions;
    }
    RepackAtlas();
}
