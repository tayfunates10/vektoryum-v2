#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

namespace vektoryum::io {

enum class RasterFormat : std::uint8_t {
    Unknown = 0,
    Png,
    Jpeg,
    Webp,
    Tiff,
};

enum class RasterInputError : std::uint8_t {
    None = 0,
    OpenFailed,
    EmptyFile,
    TooLarge,
    ReadFailed,
    UnsupportedFormat,
};

struct RasterInput {
    RasterFormat format{RasterFormat::Unknown};
    std::vector<std::uint8_t> bytes;
};

struct RasterInputResult {
    RasterInputError error{RasterInputError::None};
    RasterInput input{};

    [[nodiscard]] bool ok() const noexcept {
        return error == RasterInputError::None;
    }
};

inline constexpr std::size_t raster_input_max_bytes = 64U * 1024U * 1024U;

[[nodiscard]] RasterFormat detect_raster_format(std::span<const std::uint8_t> bytes) noexcept;
[[nodiscard]] RasterInputResult load_raster_input(
    const std::filesystem::path& path,
    std::size_t max_bytes = raster_input_max_bytes);
[[nodiscard]] std::string_view raster_format_name(RasterFormat format) noexcept;
[[nodiscard]] std::string_view raster_input_error_name(RasterInputError error) noexcept;

}  // namespace vektoryum::io
