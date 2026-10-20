#include "stdafx.h"
#include "ModelAsset.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
#define CGLTF_IMPLEMENTATION
#include "Dependencies/cgltf/cgltf.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <limits>
#include <stdexcept>

namespace Models
{
    namespace
    {
        constexpr size_t MaxFileBytes = 256 * 1024 * 1024;
        constexpr size_t MaxFloats = 32 * 1024 * 1024;

        void Require(bool condition, const char* message)
        {
            if (!condition)
            {
                throw std::runtime_error(message);
            }
        }

        std::string Name(const char* name)
        {
            return name ? name : "";
        }

        template <class T> int Index(const T* value, const T* base)
        {
            return value ? static_cast<int>(value - base) : -1;
        }

        cgltf_result ReadFile(const cgltf_memory_options* memory,
                              const cgltf_file_options*,
                              const char* path,
                              cgltf_size* size,
                              void** output)
        {
            try
            {
                std::ifstream file(std::filesystem::u8path(path), std::ios::binary | std::ios::ate);
                if (!file)
                {
                    return cgltf_result_file_not_found;
                }

                const auto length = file.tellg();
                if (length <= 0 || static_cast<std::uint64_t>(length) > MaxFileBytes)
                {
                    return cgltf_result_io_error;
                }

                const size_t count = *size ? *size : static_cast<size_t>(length);
                if (count > static_cast<size_t>(length) || count > MaxFileBytes)
                {
                    return cgltf_result_io_error;
                }

                void* bytes = memory->alloc_func ? memory->alloc_func(memory->user_data, count)
                                                 : std::malloc(count);
                if (!bytes)
                {
                    return cgltf_result_out_of_memory;
                }

                file.seekg(0);
                file.read(static_cast<char*>(bytes), static_cast<std::streamsize>(count));
                if (!file)
                {
                    if (memory->free_func)
                    {
                        memory->free_func(memory->user_data, bytes);
                    }
                    else
                    {
                        std::free(bytes);
                    }
                    return cgltf_result_io_error;
                }

                *output = bytes;
                *size = count;
                return cgltf_result_success;
            }
            catch (...)
            {
                return cgltf_result_io_error;
            }
        }

        void ReleaseFile(const cgltf_memory_options* memory, const cgltf_file_options*, void* bytes)
        {
            if (memory->free_func)
            {
                memory->free_func(memory->user_data, bytes);
            }
            else
            {
                std::free(bytes);
            }
        }

        std::vector<float> Floats(const cgltf_accessor* accessor, size_t components)
        {
            Require(accessor && cgltf_num_components(accessor->type) == components,
                    "Unexpected accessor element type");
            Require(accessor->count <= MaxFloats / components, "Accessor exceeds loader limit");
            std::vector<float> result(accessor->count * components);
            Require(cgltf_accessor_unpack_floats(accessor, result.data(), result.size())
                        == result.size(),
                    "Could not unpack accessor");
            for (float value : result)
            {
                Require(std::isfinite(value), "Non-finite accessor value");
            }
            return result;
        }

        TextureSlot ReadTexture(const cgltf_texture_view& view, const cgltf_data& data)
        {
            TextureSlot slot;
            slot.texture = Index(view.texture, data.textures);
            slot.texcoord = view.texcoord;
            slot.strength = view.scale;
            if (view.has_transform)
            {
                std::copy_n(view.transform.offset, 2, slot.offset.begin());
                std::copy_n(view.transform.scale, 2, slot.scale.begin());
                slot.rotation = view.transform.rotation;
                if (view.transform.has_texcoord)
                {
                    slot.texcoord = view.transform.texcoord;
                }
            }
            return slot;
        }

        Primitive ReadPrimitive(const cgltf_primitive& source, const cgltf_data& data)
        {
            Require(source.type == cgltf_primitive_type_triangles,
                    "Only triangle primitives are supported; export triangulated meshes");
            Require(!source.has_draco_mesh_compression && source.targets_count == 0,
                    "Draco and morph targets require a separate decoder");
            const cgltf_accessor* positions = nullptr;
            for (size_t i = 0; i < source.attributes_count; ++i)
            {
                if (source.attributes[i].type == cgltf_attribute_type_position)
                {
                    positions = source.attributes[i].data;
                }
            }
            const auto positionValues = Floats(positions, 3);
            Primitive result;
            result.material = Index(source.material, data.materials);
            result.vertices.resize(positions->count);
            for (size_t i = 0; i < result.vertices.size(); ++i)
            {
                std::copy_n(positionValues.data() + i * 3, 3, result.vertices[i].position.begin());
            }

            bool normals = false;
            bool joints[2]{};
            bool weights[2]{};
            for (size_t a = 0; a < source.attributes_count; ++a)
            {
                const auto& attribute = source.attributes[a];
                Require(attribute.data->count == result.vertices.size(),
                        "Attribute count mismatch");
                const bool influence = attribute.type == cgltf_attribute_type_joints
                                       || attribute.type == cgltf_attribute_type_weights;
                if (influence)
                {
                    Require(attribute.index >= 0 && attribute.index < 2,
                            "More than eight bone influences are unsupported");
                }
                if ((attribute.type == cgltf_attribute_type_texcoord && attribute.index != 0)
                    || (attribute.type == cgltf_attribute_type_color && attribute.index != 0))
                {
                    throw std::runtime_error("Only TEXCOORD_0 and COLOR_0 are supported");
                }
                if (attribute.type != cgltf_attribute_type_normal
                    && attribute.type != cgltf_attribute_type_texcoord
                    && attribute.type != cgltf_attribute_type_color && !influence)
                {
                    continue;
                }
                const size_t count = cgltf_num_components(attribute.data->type);
                const auto values = Floats(attribute.data, count);
                Require((attribute.type == cgltf_attribute_type_normal && count == 3)
                            || (attribute.type == cgltf_attribute_type_texcoord && count == 2)
                            || (attribute.type == cgltf_attribute_type_color
                                && (count == 3 || count == 4))
                            || (influence && count == 4),
                        "Invalid vertex attribute shape");
                for (size_t i = 0; i < result.vertices.size(); ++i)
                {
                    auto& vertex = result.vertices[i];
                    const float* value = values.data() + i * count;
                    if (attribute.type == cgltf_attribute_type_normal)
                    {
                        std::copy_n(value, 3, vertex.normal.begin());
                        normals = true;
                    }
                    else if (attribute.type == cgltf_attribute_type_texcoord)
                    {
                        std::copy_n(value, 2, vertex.uv.begin());
                    }
                    else if (attribute.type == cgltf_attribute_type_color)
                    {
                        std::copy_n(value, count, vertex.color.begin());
                    }
                    else if (attribute.type == cgltf_attribute_type_joints)
                    {
                        for (size_t j = 0; j < 4; ++j)
                        {
                            Require(value[j] >= 0 && value[j] <= 65535
                                        && std::floor(value[j]) == value[j],
                                    "Invalid joint index");
                            vertex.joints[attribute.index * 4 + j] =
                                static_cast<std::uint16_t>(value[j]);
                        }
                        joints[attribute.index] = true;
                    }
                    else
                    {
                        std::copy_n(value, 4, vertex.weights.begin() + attribute.index * 4);
                        weights[attribute.index] = true;
                    }
                }
            }
            Require(joints[0] == weights[0] && joints[1] == weights[1] && (!joints[1] || joints[0]),
                    "Joint and weight attribute sets must match");
            result.skinned = joints[0];
            for (auto& vertex : result.vertices)
            {
                if (result.skinned)
                {
                    float sum = 0;
                    for (float weight : vertex.weights)
                    {
                        Require(weight >= 0, "Negative skin weight");
                        sum += weight;
                    }
                    Require(sum > .000001f, "Skinned vertex has no weights");
                    for (float& weight : vertex.weights)
                    {
                        weight /= sum;
                    }
                }
            }

            if (source.indices)
            {
                Require(!source.indices->is_sparse, "Sparse index buffers are unsupported");
                Require(source.indices->count <= MaxFloats, "Index count exceeds loader limit");
                result.indices.reserve(source.indices->count);
                for (size_t i = 0; i < source.indices->count; ++i)
                {
                    const auto index = cgltf_accessor_read_index(source.indices, i);
                    Require(index < result.vertices.size(), "Index outside vertex buffer");
                    result.indices.push_back(static_cast<std::uint32_t>(index));
                }
            }
            else
            {
                for (size_t i = 0; i < result.vertices.size(); ++i)
                {
                    result.indices.push_back(static_cast<std::uint32_t>(i));
                }
            }
            Require(result.indices.size() % 3 == 0,
                    "Triangle index count is not divisible by three");
            if (!normals)
            {
                for (size_t i = 0; i < result.indices.size(); i += 3)
                {
                    auto& a = result.vertices[result.indices[i]];
                    auto& b = result.vertices[result.indices[i + 1]];
                    auto& c = result.vertices[result.indices[i + 2]];
                    const auto& p = a.position;
                    const auto& q = b.position;
                    const auto& r = c.position;
                    const std::array<float, 3> normal{
                        (q[1] - p[1]) * (r[2] - p[2]) - (q[2] - p[2]) * (r[1] - p[1]),
                        (q[2] - p[2]) * (r[0] - p[0]) - (q[0] - p[0]) * (r[2] - p[2]),
                        (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])};
                    for (size_t j = 0; j < 3; ++j)
                    {
                        a.normal[j] += normal[j];
                        b.normal[j] += normal[j];
                        c.normal[j] += normal[j];
                    }
                }
            }
            for (auto& vertex : result.vertices)
            {
                const auto& n = vertex.normal;
                const float length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
                if (length > .000001f)
                {
                    for (float& value : vertex.normal)
                    {
                        value /= length;
                    }
                }
            }
            return result;
        }
    } // namespace

    std::shared_ptr<Asset> LoadGltf(const std::filesystem::path& path)
    {
        cgltf_options options{};
        options.file.read = ReadFile;
        options.file.release = ReleaseFile;
        cgltf_data* parsed = nullptr;
        const std::string filename = path.u8string();
        const auto parseResult = cgltf_parse_file(&options, filename.c_str(), &parsed);
        const std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data(parsed, cgltf_free);
        Require(parseResult == cgltf_result_success, "Could not parse glTF/GLB file");
        for (size_t i = 0; i < data->extensions_required_count; ++i)
        {
            const std::string extension = data->extensions_required[i];
            Require(extension == "KHR_materials_emissive_strength"
                        || extension == "KHR_materials_unlit"
                        || extension == "KHR_texture_transform",
                    "Asset requires an unsupported glTF extension");
        }
        size_t totalBytes = 0;
        for (size_t i = 0; i < data->buffers_count; ++i)
        {
            Require(data->buffers[i].size <= MaxFileBytes - totalBytes,
                    "Buffer data exceeds loader limit");
            totalBytes += data->buffers[i].size;
        }
        Require(cgltf_load_buffers(&options, data.get(), filename.c_str()) == cgltf_result_success,
                "Could not load glTF buffers");
        Require(cgltf_validate(data.get()) == cgltf_result_success,
                "Invalid glTF structure or accessors");
        for (size_t i = 0; i < data->buffer_views_count; ++i)
        {
            Require(!data->buffer_views[i].has_meshopt_compression,
                    "Meshopt decoder is unavailable");
        }

        auto asset = std::make_shared<Asset>();
        asset->source = path;
        for (size_t i = 0; i < data->images_count; ++i)
        {
            const auto& input = data->images[i];
            Image image;
            image.name = Name(input.name);
            image.uri = Name(input.uri);
            image.mimeType = Name(input.mime_type);
            if (input.buffer_view)
            {
                const auto* bytes = cgltf_buffer_view_data(input.buffer_view);
                Require(bytes != nullptr, "Missing embedded image buffer");
                image.embeddedBytes.assign(bytes, bytes + input.buffer_view->size);
            }
            asset->images.push_back(std::move(image));
        }
        for (size_t i = 0; i < data->textures_count; ++i)
        {
            const auto& input = data->textures[i];
            Texture texture;
            texture.image = Index(input.image, data->images);
            if (input.sampler)
            {
                texture.magFilter = input.sampler->mag_filter;
                texture.minFilter = input.sampler->min_filter;
                texture.wrapS = input.sampler->wrap_s;
                texture.wrapT = input.sampler->wrap_t;
            }
            asset->textures.push_back(texture);
        }
        for (size_t i = 0; i < data->materials_count; ++i)
        {
            const auto& input = data->materials[i];
            Material material;
            material.name = Name(input.name);
            if (input.has_pbr_metallic_roughness)
            {
                const auto& pbr = input.pbr_metallic_roughness;
                std::copy_n(pbr.base_color_factor, 4, material.baseColor.begin());
                material.metallic = pbr.metallic_factor;
                material.roughness = pbr.roughness_factor;
                material.baseColorTexture = ReadTexture(pbr.base_color_texture, *data);
                material.metallicRoughnessTexture =
                    ReadTexture(pbr.metallic_roughness_texture, *data);
            }
            const float strength =
                input.has_emissive_strength ? input.emissive_strength.emissive_strength : 1;
            for (int j = 0; j < 3; ++j)
            {
                material.emissive[j] = input.emissive_factor[j] * strength;
            }
            material.alphaMode = static_cast<int>(input.alpha_mode);
            material.alphaCutoff = input.alpha_cutoff;
            material.doubleSided = input.double_sided != 0;
            material.unlit = input.unlit != 0;
            material.normalTexture = ReadTexture(input.normal_texture, *data);
            material.occlusionTexture = ReadTexture(input.occlusion_texture, *data);
            material.emissiveTexture = ReadTexture(input.emissive_texture, *data);
            asset->materials.push_back(std::move(material));
        }
        for (size_t i = 0; i < data->meshes_count; ++i)
        {
            const auto& input = data->meshes[i];
            Mesh mesh;
            mesh.name = Name(input.name);
            for (size_t j = 0; j < input.primitives_count; ++j)
            {
                mesh.primitives.push_back(ReadPrimitive(input.primitives[j], *data));
            }
            asset->meshes.push_back(std::move(mesh));
        }
        for (size_t i = 0; i < data->nodes_count; ++i)
        {
            const auto& input = data->nodes[i];
            Node node;
            node.name = Name(input.name);
            node.parent = Index(input.parent, data->nodes);
            node.mesh = Index(input.mesh, data->meshes);
            node.skin = Index(input.skin, data->skins);
            std::copy_n(input.translation, 3, node.bind.translation.begin());
            std::copy_n(input.rotation, 4, node.bind.rotation.begin());
            std::copy_n(input.scale, 3, node.bind.scale.begin());
            std::copy_n(input.matrix, 16, node.bind.matrix.begin());
            node.bind.matrixAuthored = input.has_matrix != 0;
            if (!node.name.empty())
            {
                Require(asset->nodeByName.emplace(node.name, static_cast<int>(i)).second,
                        "Duplicate node name makes catalog selection ambiguous");
            }
            asset->nodes.push_back(std::move(node));
        }
        // The GLB node array and skin palette are not necessarily in parent-first order.
        std::vector<int> marks(asset->nodes.size());
        const std::function<void(int)> visit = [&](int index)
        {
            Require(marks[index] != 1, "Cyclic node hierarchy");
            if (marks[index] == 2)
            {
                return;
            }
            marks[index] = 1;
            if (asset->nodes[index].parent >= 0)
            {
                visit(asset->nodes[index].parent);
            }
            marks[index] = 2;
            asset->evaluationOrder.push_back(index);
        };
        for (size_t i = 0; i < asset->nodes.size(); ++i)
        {
            visit(static_cast<int>(i));
        }
        const cgltf_scene* scene =
            data->scene ? data->scene : (data->scenes_count ? data->scenes : nullptr);
        if (scene)
        {
            for (size_t i = 0; i < scene->nodes_count; ++i)
            {
                asset->sceneRoots.push_back(Index(scene->nodes[i], data->nodes));
            }
        }
        else
        {
            for (size_t i = 0; i < asset->nodes.size(); ++i)
            {
                if (asset->nodes[i].parent < 0)
                {
                    asset->sceneRoots.push_back(static_cast<int>(i));
                }
            }
        }
        for (size_t i = 0; i < data->skins_count; ++i)
        {
            const auto& input = data->skins[i];
            Skin skin;
            skin.name = Name(input.name);
            skin.skeleton = Index(input.skeleton, data->nodes);
            skin.inverseBind.assign(input.joints_count, Identity());
            for (size_t j = 0; j < input.joints_count; ++j)
            {
                skin.joints.push_back(Index(input.joints[j], data->nodes));
            }
            if (input.inverse_bind_matrices)
            {
                const auto values = Floats(input.inverse_bind_matrices, 16);
                Require(input.inverse_bind_matrices->count == input.joints_count,
                        "Inverse bind count mismatch");
                for (size_t j = 0; j < input.joints_count; ++j)
                {
                    std::copy_n(values.data() + j * 16, 16, skin.inverseBind[j].begin());
                }
            }
            asset->skins.push_back(std::move(skin));
        }
        for (const auto& node : asset->nodes)
        {
            if (node.skin < 0 || node.mesh < 0)
            {
                continue;
            }
            const auto& skin = asset->skins[node.skin];
            for (const auto& primitive : asset->meshes[node.mesh].primitives)
            {
                Require(primitive.skinned, "Skinned mesh has no bone weights");
                for (const auto& vertex : primitive.vertices)
                {
                    for (size_t j = 0; j < vertex.weights.size(); ++j)
                    {
                        Require(vertex.weights[j] == 0 || vertex.joints[j] < skin.joints.size(),
                                "Joint index outside skin palette");
                    }
                }
            }
        }
        for (size_t i = 0; i < data->animations_count; ++i)
        {
            const auto& input = data->animations[i];
            Animation clip;
            clip.name = Name(input.name);
            for (size_t j = 0; j < input.channels_count; ++j)
            {
                const auto& source = input.channels[j];
                Require(source.target_node && source.sampler, "Animation has no target or sampler");
                Require(source.target_path != cgltf_animation_path_type_weights,
                        "Morph animation is unsupported");
                Require(source.target_path == cgltf_animation_path_type_translation
                            || source.target_path == cgltf_animation_path_type_rotation
                            || source.target_path == cgltf_animation_path_type_scale,
                        "Unsupported animation target path");
                Channel channel;
                channel.node = Index(source.target_node, data->nodes);
                Require(!asset->nodes[channel.node].bind.matrixAuthored,
                        "Animated node must use TRS");
                channel.path = source.target_path == cgltf_animation_path_type_rotation
                                   ? ChannelPath::Rotation
                               : source.target_path == cgltf_animation_path_type_scale
                                   ? ChannelPath::Scale
                                   : ChannelPath::Translation;
                const auto& sampler = *source.sampler;
                channel.interpolation =
                    sampler.interpolation == cgltf_interpolation_type_cubic_spline
                        ? Interpolation::CubicSpline
                    : sampler.interpolation == cgltf_interpolation_type_step
                        ? Interpolation::Step
                        : Interpolation::Linear;
                channel.times = Floats(sampler.input, 1);
                Require(!channel.times.empty() && channel.times.front() >= 0,
                        "Invalid animation timeline");
                for (size_t k = 1; k < channel.times.size(); ++k)
                {
                    Require(channel.times[k] > channel.times[k - 1],
                            "Animation keys must increase strictly");
                }
                const size_t components = channel.path == ChannelPath::Rotation ? 4 : 3;
                const auto values = Floats(sampler.output, components);
                const size_t multiplier =
                    channel.interpolation == Interpolation::CubicSpline ? 3 : 1;
                Require(sampler.output->count == channel.times.size() * multiplier,
                        "Animation key and output count mismatch");
                channel.values.resize(sampler.output->count);
                for (size_t k = 0; k < channel.values.size(); ++k)
                {
                    std::copy_n(values.data() + k * components,
                                components,
                                channel.values[k].begin());
                }
                clip.duration = (std::max)(clip.duration, channel.times.back());
                clip.channels.push_back(std::move(channel));
            }
            Require(asset->animationByName.emplace(clip.name, static_cast<int>(i)).second,
                    "Duplicate animation name");
            asset->animations.push_back(std::move(clip));
        }
        return asset;
    }
} // namespace Models
