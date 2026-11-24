#include "stdafx.h"
#include "ProgressSave.h"
#include "Prototype.h"
#include "HeroWeapons.h"
#include <Windows.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace
{
    using Json = nlohmann::json;
    constexpr auto Interval = std::chrono::seconds(120);
    constexpr size_t MaximumBytes = 64 * 1024 * 1024;

    std::string Checksum(const std::string& bytes)
    {
        std::uint64_t hash = 14695981039346656037ull;
        for (unsigned char byte : bytes)
        {
            hash = (hash ^ byte) * 1099511628211ull;
        }
        return std::to_string(hash);
    }

    void CheckNumbers(const Json& value, unsigned depth = 0)
    {
        if (depth > 64)
        {
            throw std::runtime_error("Save nesting limit exceeded");
        }
        if (value.is_number_float() && !std::isfinite(value.get<double>()))
        {
            throw std::runtime_error("Non-finite save value");
        }
        if (value.is_structured())
        {
            for (const auto& child : value)
            {
                CheckNumbers(child, depth + 1);
            }
        }
    }

    Saves::Snapshot Read(const std::filesystem::path& path)
    {
        if (std::filesystem::file_size(path) > MaximumBytes)
        {
            throw std::runtime_error("Save exceeds size limit");
        }
        std::ifstream input(path, std::ios::binary);
        const Json envelope = Json::parse(input);
        if (envelope.at("format") != "TheLastEmber" || envelope.at("version") != 1)
        {
            throw std::runtime_error("Unsupported save format/version");
        }
        const auto& payload = envelope.at("payload");
        if (envelope.at("checksum") != Checksum(payload.dump()))
        {
            throw std::runtime_error("Save checksum mismatch");
        }
        CheckNumbers(payload);
        auto state = payload.get<Saves::Snapshot>();
        state.skillCooldown = payload.value("skillCooldown", 0.0f);
        for (size_t i = 0; i < state.echoes.size(); ++i)
        {
            state.echoes[i].weapon = payload.at("echoes").at(i).value("weapon", -1);
        }
        Saves::Validate(state);
        return state;
    }

    std::runtime_error FileError(const char* operation)
    {
        return std::runtime_error(std::string(operation) + " (Windows error "
                                  + std::to_string(GetLastError()) + ")");
    }
} // namespace

ProgressSave::ProgressSave()
{
    m_NextSave = std::chrono::steady_clock::now() + Interval;
    try
    {
        wchar_t local[32768]{};
        const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", local, 32768);
        if (length == 0 || length >= 32768)
        {
            throw FileError("LOCALAPPDATA unavailable");
        }
        const auto folder = std::filesystem::path(local) / "TheLastEmber" / "Saves";
        std::filesystem::create_directories(folder);
        m_Path = folder / "autosave.json";
        const HANDLE lock = CreateFileW((folder / "autosave.lock").c_str(),
                                        GENERIC_WRITE,
                                        0,
                                        nullptr,
                                        OPEN_ALWAYS,
                                        FILE_ATTRIBUTE_NORMAL,
                                        nullptr);
        if (lock == INVALID_HANDLE_VALUE)
        {
            throw FileError("Another game session may own the save");
        }
        m_Lock = lock;
        std::cout << "[Save] path=" << m_Path.u8string() << " intervalSeconds=120\n";
        bool existing = false;
        for (bool backup : {false, true})
        {
            auto path = m_Path;
            if (backup)
            {
                path += L".bak";
            }
            if (!std::filesystem::exists(path))
            {
                continue;
            }
            existing = true;
            try
            {
                m_Load = Read(path);
                m_FromBackup = backup;
                m_BackupCurrent = !backup;
                std::cout << "[Save] loaded=" << path.u8string() << '\n';
                break;
            }
            catch (const std::exception& error)
            {
                std::cerr << "[Save] rejected=" << path.u8string() << " reason=" << error.what()
                          << '\n';
            }
        }
        if (existing && !m_Load)
        {
            throw std::runtime_error(
                "No valid save; existing files preserved and automatic writes disabled");
        }
    }
    catch (const std::exception& error)
    {
        m_Enabled = false;
        m_LoadError = error.what();
        std::cerr << "[Save] disabled: " << m_LoadError << '\n';
    }
}

ProgressSave::~ProgressSave()
{
    if (m_Worker.valid())
    {
        m_Worker.wait();
    }
    if (m_Lock)
    {
        CloseHandle(m_Lock);
    }
}

std::optional<std::uint64_t> ProgressSave::SavedSeed() const
{
    return m_Load ? std::optional<std::uint64_t>(m_Load->seed) : std::nullopt;
}

Saves::Snapshot ProgressSave::Capture(const Prototype& game)
{
    Saves::Snapshot state;
    state.seed = game.m_WorldSeed;
    state.time = game.m_Time;
    state.farmKills = game.m_FarmKills;
    state.collectedItems = game.m_CollectedItems;
    state.guardianDefeated = game.m_GuardianDefeated;
    state.levelComplete = game.m_LevelOneComplete;
    state.guardianAnnounced = game.m_GuardianAnnounced;
    state.guardian = game.m_GuardianId;
    state.nextEcho = game.m_NextEcho;
    state.player = Saves::Player::Capture(game.m_Player);
    state.skillCooldown = game.m_Player.skillCooldown;
    for (const auto& enemy : game.m_Enemies)
    {
        state.enemies.push_back(Saves::Enemy::Capture(enemy));
    }
    for (const auto& echo : game.m_Echoes)
    {
        state.echoes.push_back(Saves::Echo::Capture(echo));
    }
    for (const auto& loot : game.m_Loot)
    {
        state.loot.push_back(Saves::Loot::Capture(loot));
    }
    for (auto id : game.m_NpcWorld->m_Actors)
    {
        const auto* npc = static_cast<const NpcActor*>(game.m_Scene.Find(id));
        if (!npc)
        {
            throw std::runtime_error("NPC disappeared during snapshot capture");
        }
        state.npcs.push_back(Saves::Npc::Capture(*npc));
    }
    state.world = game.m_NpcWorld->CaptureProgress();
    return state;
}

void ProgressSave::Restore(Prototype& game)
{
    if (!m_Enabled)
    {
        game.Notice("저장 파일을 사용할 수 없습니다. 기존 파일은 보존되며 자동저장은 중지됩니다. "
                    "콘솔을 확인하세요.");
        return;
    }
    if (m_Load)
    {
        auto state = std::move(*m_Load);
        m_Load.reset();
        // All data was parsed and validated before the world was generated with its saved seed.
        std::map<ActorId, ActorId> ids{{state.player.actor, game.m_Player.Id()}};
        game.m_Time = state.time;
        game.m_FarmKills = state.farmKills;
        game.m_CollectedItems = state.collectedItems;
        game.m_GuardianDefeated = state.guardianDefeated;
        game.m_LevelOneComplete = state.levelComplete;
        game.m_GuardianAnnounced = state.guardianAnnounced;
        game.m_NextEcho = state.nextEcho;
        state.player.Apply(game.m_Player);
        game.m_Player.skillCooldown = state.skillCooldown;
        game.m_Player.range = HeroWeapons[game.m_Player.weapon].range;
        game.m_CameraX = state.player.position.x;
        game.m_CameraY = state.player.position.y;
        game.m_Enemies.clear();
        for (const auto& saved : state.enemies)
        {
            auto& actor = game.SpawnEnemy(saved.kind);
            saved.Apply(actor);
            ids.emplace(saved.actor, actor.Id());
        }
        game.m_GuardianId = ids.at(state.guardian);
        game.m_Echoes.clear();
        for (const auto& saved : state.echoes)
        {
            auto& actor = game.m_Echoes.Spawn(game,
                                              saved.id,
                                              saved.position.x,
                                              saved.position.y,
                                              saved.corruption,
                                              saved.health,
                                              saved.alive,
                                              saved.maxHealth,
                                              saved.damage,
                                              saved.level);
            saved.Apply(actor);
            ids.emplace(saved.actor, actor.Id());
        }
        game.m_Loot.clear();
        for (const auto& saved : state.loot)
        {
            auto& actor = game.m_Loot.Spawn(game,
                                            saved.position.x,
                                            saved.position.y,
                                            saved.amount,
                                            saved.type);
            saved.Apply(actor);
            ids.emplace(saved.actor, actor.Id());
        }
        auto& world = *game.m_NpcWorld;
        for (auto id : world.m_Actors)
        {
            game.m_Scene.Destroy(id);
        }
        world.m_Actors.clear();
        for (const auto& saved : state.npcs)
        {
            auto& npc = game.m_Scene.Spawn<NpcActor>(world.m_VillageRoots[saved.profile.village],
                                                     game,
                                                     world,
                                                     saved.profile,
                                                     saved.home);
            saved.Apply(npc);
            npc.mode = npc.vitals.alive ? NpcMode::Daily : NpcMode::Dead;
            npc.execution.phase = NpcPhase::Complete;
            // Replan interrupted actions and paths; never replay an already applied transaction.
            npc.nextDecision = world.Time();
            ids.emplace(saved.actor, npc.Id());
            world.m_Actors.push_back(npc.Id());
        }
        world.RestoreProgress(std::move(state.world), ids);
        game.m_Scene.CollectGarbage();
        game.Notice(m_FromBackup ? "이전 자동저장 백업에서 진행상황을 복구했습니다."
                                 : "저장된 진행상황을 불러왔습니다. 2분마다 자동저장됩니다.");
    }
    else
    {
        Start(
            game); // Establish the world seed even if the first session is shorter than two minutes.
    }
    m_NextSave = std::chrono::steady_clock::now() + Interval;
}

ProgressSave::Result
ProgressSave::Write(const std::filesystem::path& path, Saves::Snapshot state, bool backup)
{
    Result result;
    const auto started = std::chrono::steady_clock::now();
    HANDLE file = INVALID_HANDLE_VALUE;
    auto temporary = path;
    temporary += L".tmp";
    try
    {
        Saves::Validate(state);
        Json payload = state;
        payload["skillCooldown"] = state.skillCooldown;
        for (size_t i = 0; i < state.echoes.size(); ++i)
        {
            payload["echoes"][i]["weapon"] = state.echoes[i].weapon;
        }
        CheckNumbers(payload);
        const auto checksum = Checksum(payload.dump());
        Json envelope{{"format", "TheLastEmber"},
                      {"version", 1},
                      {"checksum", checksum},
                      {"payload", std::move(payload)}};
        const std::string bytes = envelope.dump();
        if (bytes.size() > MaximumBytes)
        {
            throw std::runtime_error("Save exceeds size limit");
        }
        file = CreateFileW(temporary.c_str(),
                           GENERIC_WRITE,
                           0,
                           nullptr,
                           CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL,
                           nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            throw FileError("Open temporary save");
        }
        DWORD written = 0;
        if (!WriteFile(file, bytes.data(), DWORD(bytes.size()), &written, nullptr)
            || written != bytes.size() || !FlushFileBuffers(file))
        {
            throw FileError("Write/flush temporary save");
        }
        CloseHandle(file);
        file = INVALID_HANDLE_VALUE;
        auto previous = path;
        previous += L".bak";
        if (std::filesystem::exists(path))
        {
            if (!ReplaceFileW(path.c_str(),
                              temporary.c_str(),
                              backup ? previous.c_str() : nullptr,
                              REPLACEFILE_IGNORE_MERGE_ERRORS,
                              nullptr,
                              nullptr))
            {
                throw FileError("Replace automatic save");
            }
        }
        else if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH))
        {
            throw FileError("Install automatic save");
        }
        result.success = true;
        result.bytes = bytes.size();
    }
    catch (const std::exception& error)
    {
        result.error = error.what();
    }
    if (file != INVALID_HANDLE_VALUE)
    {
        CloseHandle(file);
    }
    result.writeMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count();
    return result;
}

void ProgressSave::Start(Prototype& game)
{
    if (!m_Enabled || m_Worker.valid())
    {
        return;
    }
    try
    {
        const auto started = std::chrono::steady_clock::now();
        auto state = Capture(game);
        const double captureMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
                .count();
        // Only owned value data enters the worker. No SceneGraph, actors, or OpenGL calls.
        m_Worker = std::async(std::launch::async, Write, m_Path, std::move(state), m_BackupCurrent);
        std::cout << "[Save] started captureMs=" << captureMs << '\n';
    }
    catch (const std::exception& error)
    {
        std::cerr << "[Save] capture failed: " << error.what() << '\n';
        game.Notice("자동저장에 실패했습니다. 기존 저장 파일은 유지됩니다.");
    }
}

void ProgressSave::Complete(Prototype& game, bool wait)
{
    if (!m_Worker.valid()
        || (!wait && m_Worker.wait_for(std::chrono::seconds(0)) != std::future_status::ready))
    {
        return;
    }
    try
    {
        const auto result = m_Worker.get();
        if (!result.success)
        {
            throw std::runtime_error(result.error);
        }
        m_BackupCurrent = true;
        std::cout << "[Save] complete bytes=" << result.bytes << " writeMs=" << result.writeMs
                  << '\n';
        game.Notice("진행상황이 자동저장되었습니다.");
    }
    catch (const std::exception& error)
    {
        std::cerr << "[Save] failed: " << error.what() << '\n';
        game.Notice("자동저장에 실패했습니다. 기존 저장 파일은 유지됩니다. 콘솔을 확인하세요.");
    }
}

void ProgressSave::Tick(Prototype& game)
{
    Complete(game, false);
    const auto now = std::chrono::steady_clock::now();
    if (m_Enabled && now >= m_NextSave && !m_Worker.valid())
    {
        Start(game);
        m_NextSave = now + Interval;
    }
}

void ProgressSave::Finish(Prototype& game)
{
    Complete(game, true);
    Start(game);
    Complete(game, true);
}
