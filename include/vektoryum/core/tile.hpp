#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace vektoryum::core {

struct RectU32 {
    std::uint32_t x{};
    std::uint32_t y{};
    std::uint32_t width{};
    std::uint32_t height{};
};

struct TileRegion {
    RectU32 core{};
    RectU32 expanded{};
};

enum class TilePlanError : std::uint8_t {
    None,
    ZeroImageDimension,
    ZeroTileExtent,
    TooManyTiles,
};

struct TilePlan {
    TilePlanError error{TilePlanError::None};
    std::vector<TileRegion> tiles{};

    [[nodiscard]] bool ok() const noexcept {
        return error == TilePlanError::None;
    }
};

[[nodiscard]] TilePlan plan_tiles(
    std::uint32_t image_width,
    std::uint32_t image_height,
    std::uint32_t tile_extent,
    std::uint32_t overlap,
    std::size_t max_tiles = 1'000'000U);

}  // namespace vektoryum::core
