#pragma once
#include "SaveData.h"
#include <chrono>
#include <filesystem>
#include <future>
#include <optional>

class ProgressSave
{
public:

    ProgressSave();
    ~ProgressSave();
    std::optional<std::uint64_t> SavedSeed() const;
    void Restore(Prototype& game);
    void Tick(Prototype& game);
    void Finish(Prototype& game);

private:

    struct Result
    {
        bool success = false;
        size_t bytes = 0;
        double writeMs = 0;
        std::string error;
    };

    static Saves::Snapshot Capture(const Prototype& game);
    static Result Write(const std::filesystem::path& path, Saves::Snapshot state, bool backup);
    void Start(Prototype& game);
    void Complete(Prototype& game, bool wait);
    std::filesystem::path m_Path;
    std::optional<Saves::Snapshot> m_Load;
    std::future<Result> m_Worker;
    std::chrono::steady_clock::time_point m_NextSave;
    void* m_Lock = nullptr;
    bool m_Enabled = true;
    bool m_BackupCurrent = false;
    bool m_FromBackup = false;
    std::string m_LoadError;
};
