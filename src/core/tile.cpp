#include "vektoryum/core/tile.hpp"

#include <algorithm>
#include <limits>

namespace vektoryum::core {
namespace {

[[nodiscard]] std::uint32_t saturating_subtract(
    std::uint32_t value,
    std::uint32_t amount) noexcept {
    return amount > value ? 0U : value - amount;
}

[[nodiscard]] std::uint32_t saturating_add_bounded(
    std::uint32_t value,
    std::uint32_t amount,
    std::uint32_t bound) noexcept {
    const std::uint64_t sum = static_cast<std::uint64_t>(value) + amount;
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(sum, bound));
}

[[nodiscard]] std::size_t ceil_div_u32(std::uint32_t value, std::uint32_t divisor) noexcept {
    return static_cast<std::size_t>(
        (static_cast<std::uint64_t>(value) + divisor - 1U) / divisor);
}

}  // namespace

TilePlan plan_tiles(
    std::uint32_t image_width,
    std::uint32_t image_height,
    std::uint32_t tile_extent,
    std::uint32_t overlap,
    std::size_t max_tiles) {
    if (image_width == 0U || image_height == 0U) {
        return {TilePlanError::ZeroImageDimension, {}};
    }
    if (tile_extent == 0U) {
        return {TilePlanError::ZeroTileExtent, {}};
    }

    const std::size_t columns = ceil_div_u32(image_width, tile_extent);
    const std::size_t rows = ceil_div_u32(image_height, tile_extent);
    if (columns != 0U && rows > std::numeric_limits<std::size_t>::max() / columns) {
        return {TilePlanError::TooManyTiles, {}};
    }
    const std::size_t tile_count = columns * rows;
    if (tile_count > max_tiles) {
        return {TilePlanError::TooManyTiles, {}};
    }

    TilePlan plan;
    plan.tiles.reserve(tile_count);

    for (std::size_t row = 0; row < rows; ++row) {
        for (std::size_t column = 0; column < columns; ++column) {
            const std::uint64_t x64 = static_cast<std::uint64_t>(column) * tile_extent;
            const std::uint64_t y64 = static_cast<std::uint64_t>(row) * tile_extent;
            const auto x = static_cast<std::uint32_t>(x64);
            const auto y = static_cast<std::uint32_t>(y64);

            const std::uint32_t core_width = std::min(tile_extent, image_width - x);
            const std::uint32_t core_height = std::min(tile_extent, image_height - y);

            const std::uint32_t expanded_x = saturating_subtract(x, overlap);
            const std::uint32_t expanded_y = saturating_subtract(y, overlap);
            const std::uint32_t core_right = x + core_width;
            const std::uint32_t core_bottom = y + core_height;
            const std::uint32_t expanded_right =
                saturating_add_bounded(core_right, overlap, image_width);
            const std::uint32_t expanded_bottom =
                saturating_add_bounded(core_bottom, overlap, image_height);

            plan.tiles.push_back({
                {x, y, core_width, core_height},
                {
                    expanded_x,
                    expanded_y,
                    expanded_right - expanded_x,
                    expanded_bottom - expanded_y,
                },
            });
        }
    }

    return plan;
}

}  // namespace vektoryum::core
