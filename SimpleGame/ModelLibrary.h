#pragma once

#include "ModelAsset.h"
#include <map>

namespace Models
{
    struct ModelInstance
    {
        std::shared_ptr<const Asset> asset;
        std::vector<int> nodes;

        explicit operator bool() const
        {
            return asset && !nodes.empty();
        }
    };

    struct ActionVisual
    {
        std::string prepare;
        std::string approach;
        std::string perform;
        std::string recover;
        std::string enter;
        std::string exit;
        std::map<int, std::string> performByRole;
        float blendSeconds = .12f;
        int handEquipment = -1;
        bool overridesHand = false;
        bool visualEventsOnly = false;
    };

    // Main-thread resource service. File I/O happens on load/reload, never on animation advance.
    class ModelLibrary
    {
    public:

        bool Initialize(const std::filesystem::path& modelingRoot = {});

        bool Ready() const
        {
            return static_cast<bool>(m_NpcParts);
        }

        int SchemaVersion() const
        {
            return m_Schema;
        }

        const std::filesystem::path& PackagePath() const
        {
            return m_Package;
        }

        ModelInstance Npc(std::uint64_t id, int role) const;
        ModelInstance Player(int role = 2) const;
        ModelInstance Monster(int index) const;
        ModelInstance Prop(int propIndex) const;
        ModelInstance GameProp(int gameKind) const;
        const ActionVisual* NpcAction(int action) const;
        std::string MonsterClip(int index, const std::string& state) const;
        std::shared_ptr<const Asset> Load(const std::filesystem::path& path);

    private:

        struct Appearance
        {
            int role = 0;
            std::vector<int> parts;
        };

        bool LoadPackage(const std::filesystem::path& path);
        ModelInstance Assemble(const Appearance& appearance) const;
        std::shared_ptr<Asset> ReadCached(const std::filesystem::path& path);
        int m_Schema = 0;
        std::filesystem::path m_Package;
        std::shared_ptr<const Asset> m_NpcParts;
        std::shared_ptr<const Asset> m_Props;
        std::map<std::filesystem::path, std::shared_ptr<Asset>> m_Cache;
        std::map<std::pair<int, int>, int> m_PartNodes;
        std::map<std::string, int> m_Slots;
        std::map<int, std::pair<bool, bool>> m_Headgear;
        std::map<int, std::vector<Appearance>> m_Presets;
        std::map<std::uint64_t, Appearance> m_Appearances;
        std::map<int, int> m_Weapons;
        std::map<int, int> m_PropNodes;
        std::map<int, int> m_GameProps;
        std::map<int, ModelInstance> m_Monsters;
        std::map<int, ActionVisual> m_Actions;
        std::map<std::pair<int, std::string>, std::string> m_MonsterClips;
        size_t m_CacheHits = 0;
    };
} // namespace Models
