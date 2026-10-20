#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// glTF coordinates: meters, Y up, +Z forward. Matrices are column major.
namespace Models
{
    using Matrix = std::array<float, 16>;
    using Vector = std::array<float, 4>;

    Matrix Identity();
    Matrix Multiply(const Matrix& a, const Matrix& b);
    Matrix Inverse(const Matrix& matrix);
    Vector Slerp(Vector a, Vector b, float weight);

    struct Transform
    {
        Vector translation{0, 0, 0, 0};
        Vector rotation{0, 0, 0, 1};
        Vector scale{1, 1, 1, 0};
        Matrix matrix = Identity();
        bool matrixAuthored = false;
        Matrix LocalMatrix() const;
    };

    struct Vertex
    {
        std::array<float, 3> position{};
        std::array<float, 3> normal{};
        std::array<float, 2> uv{};
        Vector color{1, 1, 1, 1};
        std::array<std::uint16_t, 8> joints{};
        std::array<float, 8> weights{};
    };

    struct Image
    {
        std::string name;
        std::string uri;
        std::string mimeType;
        std::vector<std::uint8_t> embeddedBytes;
    };

    struct Texture
    {
        int image = -1;
        int magFilter = 0;
        int minFilter = 0;
        int wrapS = 10497;
        int wrapT = 10497;
    };

    struct TextureSlot
    {
        int texture = -1;
        int texcoord = 0;
        float strength = 1;
        std::array<float, 2> offset{};
        std::array<float, 2> scale{1, 1};
        float rotation = 0;
    };

    struct Material
    {
        std::string name;
        Vector baseColor{1, 1, 1, 1};
        Vector emissive{};
        float metallic = 1;
        float roughness = 1;
        float alphaCutoff = .5f;
        int alphaMode = 0;
        bool doubleSided = false;
        bool unlit = false;
        TextureSlot baseColorTexture;
        TextureSlot metallicRoughnessTexture;
        TextureSlot normalTexture;
        TextureSlot occlusionTexture;
        TextureSlot emissiveTexture;
    };

    struct Primitive
    {
        std::vector<Vertex> vertices;
        std::vector<std::uint32_t> indices;
        int material = -1;
        bool skinned = false;
    };

    struct Mesh
    {
        std::string name;
        std::vector<Primitive> primitives;
    };

    struct Node
    {
        std::string name;
        int parent = -1;
        int mesh = -1;
        int skin = -1;
        Transform bind;
    };

    struct Skin
    {
        std::string name;
        int skeleton = -1;
        std::vector<int> joints;
        std::vector<Matrix> inverseBind;
    };

    enum class ChannelPath
    {
        Translation,
        Rotation,
        Scale
    };
    enum class Interpolation
    {
        Linear,
        Step,
        CubicSpline
    };

    struct Channel
    {
        int node = -1;
        ChannelPath path = ChannelPath::Translation;
        Interpolation interpolation = Interpolation::Linear;
        std::vector<float> times;
        // Cubic channels store incoming tangent, value, outgoing tangent per key.
        std::vector<Vector> values;
    };

    struct AnimationEvent
    {
        std::string name;
        float time = 0;
    };

    struct Animation
    {
        std::string name;
        float duration = 0;
        bool loop = false;
        float rootYawDegrees = 0;
        std::vector<Channel> channels;
        std::vector<AnimationEvent> events;
    };

    struct Asset
    {
        std::filesystem::path source;
        std::vector<Image> images;
        std::vector<Texture> textures;
        std::vector<Material> materials;
        std::vector<Mesh> meshes;
        std::vector<Node> nodes;
        std::vector<int> evaluationOrder;
        std::vector<int> sceneRoots;
        std::vector<Skin> skins;
        std::vector<Animation> animations;
        std::unordered_map<std::string, int> nodeByName;
        std::unordered_map<std::string, int> animationByName;
    };

    // Throws an actionable error; no OpenGL context or renderer calls are needed.
    std::shared_ptr<Asset> LoadGltf(const std::filesystem::path& path);

    struct Pose
    {
        std::vector<Transform> local;
        std::vector<Matrix> world;
    };

    void SamplePose(const Asset& asset, int clip, double seconds, Pose& pose);
    void
    BlendPose(const Asset& asset, const Pose& from, const Pose& to, float weight, Pose& result);
    void UpdateWorldPose(const Asset& asset, Pose& pose);
    std::vector<Matrix> SkinMatrices(const Asset& asset, const Pose& pose, int meshNode);
    void
    SkinMatrices(const Asset& asset, const Pose& pose, int meshNode, std::vector<Matrix>& output);

    // Resource is shared; playback time/pose belong to each actor instance.
    class AnimationPlayer
    {
    public:

        explicit AnimationPlayer(std::shared_ptr<const Asset> asset);
        bool Play(const std::string& clip,
                  float fadeSeconds = .12f,
                  bool preservePhase = false,
                  bool restart = false);
        void Advance(float seconds, std::vector<AnimationEvent>& events);

        const Pose& CurrentPose() const
        {
            return m_Pose;
        }

        void SetRate(float rate);

    private:

        std::shared_ptr<const Asset> m_Asset;
        Pose m_Pose;
        Pose m_From;
        int m_Clip = -1;
        double m_Time = 0;
        float m_Rate = 1;
        float m_Fade = 0;
        float m_FadeElapsed = 0;
    };
} // namespace Models
