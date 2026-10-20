#include "stdafx.h"
#include "Renderer.h"
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <system_error>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace
{
    struct ComApartment
    {
        HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        ~ComApartment()
        {
            if (SUCCEEDED(result))
            {
                CoUninitialize();
            }
        }
    };

    std::filesystem::path SpriteAtlasPath(const char* filename)
    {
        wchar_t executable[32768] = {};
        const DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
        std::error_code error;
        const auto root = length > 0 && length < 32768
                              ? std::filesystem::path(executable).parent_path()
                              : std::filesystem::current_path(error);
        const std::filesystem::path candidates[] = {root / "Assets" / filename,
                                                    root / "../../SimpleGame/Assets" / filename,
                                                    root / "../SimpleGame/Assets" / filename,
                                                    std::filesystem::path("Assets") / filename,
                                                    std::filesystem::path("SimpleGame/Assets")
                                                        / filename};

        for (const auto& path : candidates)
        {
            if (std::filesystem::is_regular_file(path, error))
            {
                return path;
            }
        }
        return {};
    }

    bool DecodeAtlas(const std::filesystem::path& path,
                     UINT& width,
                     UINT& height,
                     std::vector<unsigned char>& pixels)
    {
        ComApartment apartment;
        if (FAILED(apartment.result) && apartment.result != RPC_E_CHANGED_MODE)
        {
            return false;
        }

        Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
        Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
        Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
        HRESULT result = CoCreateInstance(CLSID_WICImagingFactory,
                                          nullptr,
                                          CLSCTX_INPROC_SERVER,
                                          IID_PPV_ARGS(factory.GetAddressOf()));
        if (FAILED(result))
        {
            return false;
        }
        result = factory->CreateDecoderFromFilename(path.c_str(),
                                                    nullptr,
                                                    GENERIC_READ,
                                                    WICDecodeMetadataCacheOnLoad,
                                                    decoder.GetAddressOf());
        if (FAILED(result) || FAILED(decoder->GetFrame(0, frame.GetAddressOf()))
            || FAILED(frame->GetSize(&width, &height)))
        {
            return false;
        }

        // Bound allocation before multiplication; malformed or oversized assets
        // use the fallback. Transparent padding determines the cell bands.
        if (width < 4 || height < 4 || width > 8192 || height > 8192)
        {
            return false;
        }
        result = factory->CreateFormatConverter(converter.GetAddressOf());
        if (FAILED(result))
        {
            return false;
        }
        result = converter->Initialize(frame.Get(),
                                       GUID_WICPixelFormat32bppRGBA,
                                       WICBitmapDitherTypeNone,
                                       nullptr,
                                       0,
                                       WICBitmapPaletteTypeCustom);
        if (FAILED(result))
        {
            return false;
        }

        const UINT stride = width * 4;
        const UINT bytes = stride * height;
        pixels.resize(bytes);
        return SUCCEEDED(converter->CopyPixels(nullptr, stride, bytes, pixels.data()));
    }

    bool FindDividers(const std::vector<unsigned char>& pixels,
                      UINT width,
                      UINT height,
                      bool vertical,
                      unsigned bands,
                      std::vector<UINT>& boundaries)
    {
        const UINT dimension = vertical ? width : height;
        const UINT length = vertical ? height : width;
        const int radius = static_cast<int>(dimension / (bands * 4));
        boundaries.resize(bands + 1);
        boundaries.front() = 0;
        boundaries.back() = dimension;

        // Generated sheets may trim their outer padding, so exact grid divisions can
        // cut through artwork. Read nearby complete transparent lines instead;
        // this discovers padding without rewriting or resampling any PNG pixels.
        for (unsigned band = 1; band < bands; ++band)
        {
            const int nominal = static_cast<int>(dimension * band / bands);
            bool found = false;
            for (int distance = 0; distance <= radius && !found; ++distance)
            {
                for (int direction = -1; direction <= 1 && !found; direction += 2)
                {
                    const int divider = nominal + distance * direction;
                    if (divider <= static_cast<int>(boundaries[band - 1])
                        || divider >= static_cast<int>(dimension))
                    {
                        continue;
                    }

                    bool clear = true;
                    for (UINT position = 0; position < length; ++position)
                    {
                        const size_t x = vertical ? size_t(divider) : size_t(position);
                        const size_t y = vertical ? size_t(position) : size_t(divider);
                        if (pixels[(y * width + x) * 4 + 3] > 8)
                        {
                            clear = false;
                            break;
                        }
                    }
                    if (clear)
                    {
                        boundaries[band] = static_cast<UINT>(divider);
                        found = true;
                    }
                }
            }
            if (!found)
            {
                return false;
            }
        }
        return boundaries[bands - 1] < boundaries[bands];
    }

    struct SpriteSheet
    {
        const char* filename = "";
        UINT width = 0;
        UINT height = 0;
        std::vector<unsigned char> pixels;
        std::vector<UINT> columns;
        std::vector<UINT> rows;
    };

    struct AlphaBounds
    {
        UINT left = 0;
        UINT top = 0;
        UINT right = 0;
        UINT bottom = 0;

        bool Valid() const
        {
            return left < right && top < bottom;
        }
    };

    struct FrameSource
    {
        const SpriteSheet* sheet = nullptr;
        AlphaBounds bounds;
        std::array<float, 4> emission{};
        Point anchor{0.5f, 1};
        float heightScale = 1;
        unsigned sprite = 0;
        UINT packedX = 0;
        UINT packedY = 0;
    };

    bool PackFrames(const std::vector<FrameSource*>& frames, UINT limit, UINT& width, UINT& height)
    {
        constexpr UINT Gutter = 2;
        if (frames.empty() || limit <= Gutter * 2)
        {
            return false;
        }

        auto ordered = frames;
        std::stable_sort(ordered.begin(),
                         ordered.end(),
                         [](const FrameSource* a, const FrameSource* b)
                         {
                             const UINT aHeight = a->bounds.bottom - a->bounds.top;
                             const UINT bHeight = b->bounds.bottom - b->bounds.top;
                             if (aHeight != bHeight)
                             {
                                 return aHeight > bHeight;
                             }
                             return a->bounds.right - a->bounds.left
                                    > b->bounds.right - b->bounds.left;
                         });

        UINT minimumWidth = Gutter * 2;
        for (const auto* frame : ordered)
        {
            minimumWidth =
                (std::max)(minimumWidth, frame->bounds.right - frame->bounds.left + Gutter * 2);
            if (frame->bounds.bottom - frame->bounds.top + Gutter * 2 > limit)
            {
                return false;
            }
        }
        if (minimumWidth > limit)
        {
            return false;
        }

        // Shelf-pack cropped artwork, not entire mostly transparent source sheets.
        // Try several shelf widths at startup and retain the smallest resident image.
        // Two clear pixels surround every crop; no mipmaps or per-frame repacking.
        std::uint64_t bestArea = 0;
        std::vector<std::array<UINT, 2>> positions(ordered.size());
        std::vector<std::array<UINT, 2>> bestPositions;
        for (UINT candidate = minimumWidth;; candidate = (std::min)(candidate + 128, limit))
        {
            UINT x = Gutter;
            UINT y = Gutter;
            UINT shelfHeight = 0;
            UINT usedWidth = 0;
            bool fits = true;
            for (size_t index = 0; index < ordered.size(); ++index)
            {
                const auto& bounds = ordered[index]->bounds;
                const UINT cropWidth = bounds.right - bounds.left;
                const UINT cropHeight = bounds.bottom - bounds.top;
                if (x + cropWidth + Gutter > candidate)
                {
                    x = Gutter;
                    y += shelfHeight + Gutter * 2;
                    shelfHeight = 0;
                }
                if (y + cropHeight + Gutter > limit)
                {
                    fits = false;
                    break;
                }
                positions[index] = {x, y};
                usedWidth = (std::max)(usedWidth, x + cropWidth + Gutter);
                shelfHeight = (std::max)(shelfHeight, cropHeight);
                x += cropWidth + Gutter * 2;
            }

            const UINT usedHeight = y + shelfHeight + Gutter;
            const std::uint64_t area = std::uint64_t(usedWidth) * usedHeight;
            if (fits
                && (bestArea == 0 || area < bestArea
                    || (area == bestArea
                        && (std::max)(usedWidth, usedHeight) < (std::max)(width, height))))
            {
                width = usedWidth;
                height = usedHeight;
                bestArea = area;
                bestPositions = positions;
            }
            if (candidate == limit)
            {
                break;
            }
        }
        if (bestArea == 0)
        {
            return false;
        }
        for (size_t index = 0; index < ordered.size(); ++index)
        {
            ordered[index]->packedX = bestPositions[index][0];
            ordered[index]->packedY = bestPositions[index][1];
        }
        return true;
    }

    AlphaBounds CellBounds(const SpriteSheet& sheet, unsigned column, unsigned row)
    {
        const UINT left = sheet.columns[column];
        const UINT right = sheet.columns[column + 1];
        const UINT top = sheet.rows[row];
        const UINT bottom = sheet.rows[row + 1];
        AlphaBounds result{right, bottom, left, top};
        for (UINT y = top; y < bottom; ++y)
        {
            for (UINT x = left; x < right; ++x)
            {
                if (sheet.pixels[(size_t(y) * sheet.width + x) * 4 + 3] > 8)
                {
                    result.left = (std::min)(result.left, x);
                    result.top = (std::min)(result.top, y);
                    result.right = (std::max)(result.right, x + 1);
                    result.bottom = (std::max)(result.bottom, y + 1);
                }
            }
        }
        return result;
    }

    std::array<float, 4>
    LanternRegion(const SpriteSheet& sheet, const AlphaBounds& body, bool detachedArm = false)
    {
        // The human player's lantern is held to image-right, below the shoulder.
        // Detect its bright yellow glass inside that small area once at startup;
        // leather, the face and metal highlights elsewhere cannot become emitters.
        const UINT width = body.right - body.left;
        const UINT height = body.bottom - body.top;
        const UINT left = detachedArm ? 0 : UINT(width * 0.68f);
        const UINT top = UINT(height * (detachedArm ? 0.50f : 0.30f));
        const UINT bottom = detachedArm ? height : UINT(height * 0.80f);
        std::vector<unsigned char> candidates(size_t(width) * height, 0);
        for (UINT y = top; y < bottom; ++y)
        {
            for (UINT x = left; x < width; ++x)
            {
                const auto pixel =
                    sheet.pixels.data() + (size_t(body.top + y) * sheet.width + body.left + x) * 4;
                if (pixel[3] >= 128 && pixel[0] >= 205 && pixel[1] >= 105
                    && int(pixel[0]) - pixel[1] >= 18 && int(pixel[1]) - pixel[2] >= 28)
                {
                    candidates[size_t(y) * width + x] = 1;
                }
            }
        }

        // Keep the largest connected glass highlight, excluding isolated bright
        // buckles elsewhere in the hand region. This avoids frame-to-frame
        // lantern flicker when a single gold pixel would expand a global box.
        AlphaBounds light;
        unsigned bestCount = 2;
        std::vector<size_t> pending;
        for (size_t start = 0; start < candidates.size(); ++start)
        {
            if (!candidates[start])
            {
                continue;
            }
            candidates[start] = 0;
            pending.push_back(start);
            AlphaBounds component{width, height, 0, 0};
            unsigned count = 0;
            while (!pending.empty())
            {
                const size_t current = pending.back();
                pending.pop_back();
                const UINT x = UINT(current % width);
                const UINT y = UINT(current / width);
                component.left = (std::min)(component.left, x);
                component.top = (std::min)(component.top, y);
                component.right = (std::max)(component.right, x + 1);
                component.bottom = (std::max)(component.bottom, y + 1);
                ++count;
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        const int neighborX = int(x) + dx;
                        const int neighborY = int(y) + dy;
                        if (neighborX < 0 || neighborY < 0 || neighborX >= int(width)
                            || neighborY >= int(height))
                        {
                            continue;
                        }
                        const size_t neighbor = size_t(neighborY) * width + neighborX;
                        if (candidates[neighbor])
                        {
                            candidates[neighbor] = 0;
                            pending.push_back(neighbor);
                        }
                    }
                }
            }
            if (count > bestCount
                && component.right - component.left <= width * (detachedArm ? 0.60f : 0.24f)
                && component.bottom - component.top <= height * (detachedArm ? 0.45f : 0.26f))
            {
                light = component;
                bestCount = count;
            }
        }
        if (!light.Valid())
        {
            return {};
        }
        // Include the other side of the glass separated by its dark metal cage.
        light.left = light.left > 2 ? light.left - 2 : 0;
        light.top = light.top > 2 ? light.top - 2 : 0;
        light.right = (std::min)(light.right + 2, width);
        light.bottom = (std::min)(light.bottom + 2, height);
        return {float(light.left) / width,
                float(light.top) / height,
                float(light.right) / width,
                float(light.bottom) / height};
    }

    bool LoadSheet(SpriteSheet& sheet, const char* filename, unsigned columns, unsigned rows)
    {
        sheet.filename = filename;
        const auto path = SpriteAtlasPath(filename);
        if (path.empty() || !DecodeAtlas(path, sheet.width, sheet.height, sheet.pixels)
            || !FindDividers(sheet.pixels, sheet.width, sheet.height, true, columns, sheet.columns)
            || !FindDividers(sheet.pixels, sheet.width, sheet.height, false, rows, sheet.rows))
        {
            std::cerr << "[Assets] " << filename << " unavailable or invalid " << columns << "x"
                      << rows << " grid; retaining available fallback poses.\n";
            sheet = {};
            return false;
        }
        return true;
    }
} // namespace

void Renderer::InitializeSpriteAtlas()
{
    // Stable articulated pieces replace independently generated whole-body frames.
    // All parts are cropped/packed once and use the existing ordered instance batch.
    std::array<SpriteSheet, 3> sheets;
    LoadSheet(sheets[0], "realistic-atlas-v1.png", 4, 4);
    LoadSheet(sheets[1], "unit-rigs-humans-v3.png", 5, 6);
    LoadSheet(sheets[2], "unit-rigs-monsters-v3.png", 5, 4);

    std::array<FrameSource, 16> staticFrames;
    if (!sheets[0].pixels.empty())
    {
        const auto& sheet = sheets[0];
        for (unsigned sprite = 0; sprite < staticFrames.size(); ++sprite)
        {
            const auto bounds = CellBounds(sheet, sprite % 4, sprite / 4);
            if (!bounds.Valid())
            {
                continue;
            }
            staticFrames[sprite].sheet = &sheet;
            staticFrames[sprite].bounds = bounds;
            staticFrames[sprite].sprite = sprite;
        }
    }

    constexpr size_t PartCount = static_cast<size_t>(RigPartId::Count);
    std::array<std::array<FrameSource, PartCount>, 10> rigFrames;
    std::array<bool, 10> validRigs{};
    // Shoulder anchors include attached equipment; the staff and bow deliberately
    // have off-center pivots. Values refer to the alpha-cropped source parts.
    constexpr Point FarShoulders[] = {{0.25f, 0.09f},
                                      {0.24f, 0.23f},
                                      {0.22f, 0.10f},
                                      {0.27f, 0.12f},
                                      {0.27f, 0.10f},
                                      {0.45f, 0.10f},
                                      {0.72f, 0.12f},
                                      {0.36f, 0.13f},
                                      {0.68f, 0.12f},
                                      {0.63f, 0.12f}};
    constexpr Point NearShoulders[] = {{0.22f, 0.13f},
                                       {0.40f, 0.12f},
                                       {0.24f, 0.12f},
                                       {0.30f, 0.13f},
                                       {0.29f, 0.12f},
                                       {0.39f, 0.12f},
                                       {0.24f, 0.12f},
                                       {0.33f, 0.12f},
                                       {0.35f, 0.12f},
                                       {0.29f, 0.12f}};
    constexpr float FarArmHeights[] =
        {0.53f, 0.61f, 0.46f, 0.44f, 0.50f, 0.41f, 0.48f, 0.53f, 0.52f, 0.43f};
    constexpr float NearArmHeights[] =
        {0.44f, 0.42f, 0.55f, 0.42f, 0.42f, 0.44f, 0.48f, 0.44f, 0.53f, 0.55f};
    for (unsigned sprite = 0; sprite < rigFrames.size(); ++sprite)
    {
        const auto& sheet = sheets[sprite < 6 ? 1 : 2];
        if (sheet.pixels.empty())
        {
            continue;
        }
        const unsigned row = sprite < 6 ? sprite : sprite - 6;
        std::array<AlphaBounds, 5> cells;
        bool complete = true;
        for (unsigned column = 0; column < cells.size(); ++column)
        {
            cells[column] = CellBounds(sheet, column, row);
            complete =
                complete && cells[column].Valid() && cells[column].bottom - cells[column].top >= 16;
        }
        if (!complete)
        {
            std::cerr << "[Assets] Incomplete rig sprite=" << sprite
                      << "; retaining static pose.\n";
            continue;
        }
        const auto makePart = [&](RigPartId id,
                                  unsigned column,
                                  float top,
                                  float bottom,
                                  Point start,
                                  Point end,
                                  float visualHeight)
        {
            const size_t index = static_cast<size_t>(id);
            auto& source = rigFrames[sprite][index];
            auto& part = m_UnitRigs[sprite].parts[index];
            const auto full = cells[column];
            const float fullHeight = float(full.bottom - full.top);
            source.sheet = &sheet;
            source.bounds = full;
            source.bounds.top += UINT(top * fullHeight);
            source.bounds.bottom = full.top + UINT(bottom * fullHeight);
            const float cropHeight = float(source.bounds.bottom - source.bounds.top);
            const float offset = float(source.bounds.top - full.top);
            part.start = {start.x, (start.y * fullHeight - offset) / cropHeight};
            part.end = {end.x, (end.y * fullHeight - offset) / cropHeight};
            source.anchor = part.start;
            source.heightScale = visualHeight * cropHeight / fullHeight;
            source.sprite = 32 + sprite;
        };
        // Overlap at waist and knees hides seams while separate transforms allow
        // counter-rotation. Both pieces share exactly the same joint coordinates.
        makePart(RigPartId::Chest, 0, 0, 0.83f, {0.5f, 0.76f}, {0.5f, 0.76f}, 0.60f);
        makePart(RigPartId::Pelvis, 0, 0.72f, 1, {0.5f, 0.88f}, {0.5f, 0.88f}, 0.60f);
        makePart(RigPartId::FarArm,
                 1,
                 0,
                 1,
                 FarShoulders[sprite],
                 {0.5f, 1},
                 FarArmHeights[sprite]);
        makePart(RigPartId::NearArm,
                 4,
                 0,
                 1,
                 NearShoulders[sprite],
                 {0.5f, 1},
                 NearArmHeights[sprite]);
        for (unsigned side = 0; side < 2; ++side)
        {
            const unsigned column = 2 + side;
            const float h = float(cells[column].bottom - cells[column].top);
            const float overlap = 2 / h;
            makePart(side == 0 ? RigPartId::FarThigh : RigPartId::NearThigh,
                     column,
                     0,
                     0.55f + overlap,
                     {0.47f, 0.08f},
                     {0.55f, 0.55f},
                     1);
            makePart(side == 0 ? RigPartId::FarShin : RigPartId::NearShin,
                     column,
                     0.55f - overlap,
                     1,
                     {0.55f, 0.55f},
                     {0.47f, 0.97f},
                     1);
        }
        if (sprite == 0)
        {
            auto& lantern = rigFrames[sprite][static_cast<size_t>(RigPartId::NearArm)];
            lantern.emission = LanternRegion(sheet, lantern.bounds, true);
        }
        validRigs[sprite] = true;
    }

    std::vector<FrameSource*> frames;
    const auto collectFrames = [&]()
    {
        frames.clear();
        for (auto& frame : staticFrames)
        {
            if (frame.bounds.Valid())
            {
                frames.push_back(&frame);
            }
        }
        for (size_t sprite = 0; sprite < rigFrames.size(); ++sprite)
        {
            if (validRigs[sprite])
            {
                for (auto& part : rigFrames[sprite])
                {
                    frames.push_back(&part);
                }
            }
        }
    };
    collectFrames();

    UINT width = 0;
    UINT height = 0;
    GLint maximumSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumSize);
    const UINT limit = maximumSize > 0 ? (std::min)(UINT(maximumSize), UINT(8192)) : 0;
    if (!PackFrames(frames, limit, width, height))
    {
        std::cerr << "[Assets] Rig parts exceed texture limit=" << limit
                  << "; retaining static sprite atlas.\n";
        validRigs.fill(false);
        collectFrames();
        if (!PackFrames(frames, limit, width, height))
        {
            std::cerr << "[Assets] No compatible sprite atlas; using procedural visuals.\n";
            return;
        }
    }

    std::vector<unsigned char> pixels(size_t(width) * height * 4, 0);
    size_t croppedPixels = 0;
    for (const auto* frame : frames)
    {
        const auto& sheet = *frame->sheet;
        const auto& bounds = frame->bounds;
        const UINT cropWidth = bounds.right - bounds.left;
        const UINT cropHeight = bounds.bottom - bounds.top;
        croppedPixels += size_t(cropWidth) * cropHeight;
        for (UINT y = 0; y < cropHeight; ++y)
        {
            const auto* source =
                sheet.pixels.data() + (size_t(bounds.top + y) * sheet.width + bounds.left) * 4;
            auto* destination =
                pixels.data() + (size_t(frame->packedY + y) * width + frame->packedX) * 4;
            std::copy_n(source, size_t(cropWidth) * 4, destination);
        }
    }

    const auto makeRegion = [width, height](const FrameSource& source)
    {
        const UINT cropWidth = source.bounds.right - source.bounds.left;
        const UINT cropHeight = source.bounds.bottom - source.bounds.top;
        SpriteRegion region;
        region.index = source.sprite;
        // WIC and unit-quad Y both increase downward. Half-texel insets keep
        // linear sampling inside this crop and outside neighboring frames.
        region.uv = {(source.packedX + 0.5f) / width,
                     (source.packedY + 0.5f) / height,
                     (source.packedX + cropWidth - 0.5f) / width,
                     (source.packedY + cropHeight - 0.5f) / height};
        region.aspect = float(cropWidth) / float(cropHeight);
        region.heightScale = source.heightScale;
        region.anchor = source.anchor;
        region.emission = source.emission;
        return region;
    };

    unsigned validRegions = 0;
    for (unsigned sprite = 0; sprite < staticFrames.size(); ++sprite)
    {
        if (staticFrames[sprite].bounds.Valid())
        {
            m_SpriteRegions[sprite] = makeRegion(staticFrames[sprite]);
            ++validRegions;
        }
    }
    unsigned rigCount = 0;
    unsigned lanternParts = 0;
    for (size_t sprite = 0; sprite < rigFrames.size(); ++sprite)
    {
        if (!validRigs[sprite])
        {
            continue;
        }
        for (size_t part = 0; part < PartCount; ++part)
        {
            const auto& source = rigFrames[sprite][part];
            m_UnitRigs[sprite].parts[part].region = makeRegion(source);
            lanternParts += source.emission[2] > source.emission[0] ? 1 : 0;
        }
        for (size_t first :
             {static_cast<size_t>(RigPartId::FarThigh), static_cast<size_t>(RigPartId::NearThigh)})
        {
            auto& thigh = m_UnitRigs[sprite].parts[first];
            auto& shin = m_UnitRigs[sprite].parts[first + 1];
            const auto sourceLength = [](const RigPart& part)
            {
                return part.region.heightScale
                       * std::hypot((part.end.x - part.start.x) * part.region.aspect,
                                    part.end.y - part.start.y);
            };
            const float upper = sourceLength(thigh);
            const float lower = sourceLength(shin);
            const float scale = 0.485f / (upper + lower);
            thigh.boneLength = upper * scale;
            shin.boneLength = lower * scale;
        }
        m_UnitRigs[sprite].ready = true;
        ++rigCount;
    }

    glGenTextures(1, &m_SpriteAtlas);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_SpriteAtlas);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA8,
                 static_cast<GLsizei>(width),
                 static_cast<GLsizei>(height),
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 pixels.data());
    GLint uploadedWidth = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &uploadedWidth);
    glActiveTexture(GL_TEXTURE0);
    if (uploadedWidth != static_cast<GLint>(width))
    {
        glDeleteTextures(1, &m_SpriteAtlas);
        m_SpriteAtlas = 0;
        std::cerr << "[Assets] Sprite atlas upload failed; using procedural visuals.\n";
        return;
    }

    // Warm the shared geometry during startup as well; drawing a new cell never
    // creates another mesh or reads another image from disk.
    if (IsInitialized())
    {
        GetMesh("sprite/unit-quad-v1",
                {0, 0},
                1,
                [this]()
                {
                    Rect(0, 0, 1, 1, Color());
                });
    }

    std::cout << "[Assets] Packed sprite atlas size=" << width << "x" << height
              << " staticCells=" << validRegions << " unitRigs=" << rigCount
              << " rigParts=" << rigCount * PartCount << " lanternParts=" << lanternParts
              << " residentBytes=" << pixels.size() << " cropBytes=" << croppedPixels * 4
              << " textureLimit=" << limit
              << " (one texture, shared quad, ordered instancing, startup upload only).\n";
    for (unsigned sprite = 0; sprite < m_UnitRigs.size(); ++sprite)
    {
        std::cout << "[Animation] Unit sprite=" << sprite
                  << " mode=" << (m_UnitRigs[sprite].ready ? "continuous-rig" : "static-fallback")
                  << " parts=" << (m_UnitRigs[sprite].ready ? PartCount : 1)
                  << " stride=3.6 maxCyclesPerSecond=1.8 shoulderCounterMotion=1\n";
    }
}

bool Renderer::DrawSprite(unsigned sprite, Point feet, float height, Color tint)
{
    if (sprite >= m_SpriteRegions.size())
    {
        return false;
    }
    return DrawSpriteRegion(m_SpriteRegions[sprite], feet, height, false, tint);
}

bool Renderer::DrawUnitSprite(unsigned sprite,
                              Point feet,
                              float height,
                              const UnitSpritePose& pose,
                              Color tint)
{
    if (sprite >= m_UnitRigs.size())
    {
        return false;
    }
    if (m_UnitRigs[sprite].ready)
    {
        return DrawUnitRig(sprite, feet, height, pose, tint);
    }
    return DrawSpriteRegion(m_SpriteRegions[sprite], feet, height, pose.facingLeft, tint);
}

bool Renderer::DrawSpriteRegion(const SpriteRegion& region,
                                Point feet,
                                float height,
                                bool facingLeft,
                                Color tint)
{
    if (!m_SpriteAtlas || !IsInitialized() || m_Capturing || region.aspect <= 0
        || !std::isfinite(height) || height <= 0 || !std::isfinite(feet.x)
        || !std::isfinite(feet.y))
    {
        return false;
    }

    height *= region.heightScale;
    const float width = height * region.aspect;
    const float horizontal = facingLeft ? -width : width;
    const auto mesh = GetMesh("sprite/unit-quad-v1",
                              {0, 0},
                              1,
                              [this]()
                              {
                                  Rect(0, 0, 1, 1, Color());
                              });
    Submit(*mesh,
           {{horizontal, 0},
            {0, height},
            {feet.x - horizontal * region.anchor.x, feet.y - height * region.anchor.y}},
           tint,
           &region);
    return true;
}
