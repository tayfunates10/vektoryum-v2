#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <string_view>
#include <utility>
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

[[nodiscard]] inline bool raster_signature_starts_with(
    std::span<const std::uint8_t> bytes,
    std::span<const std::uint8_t> signature) noexcept {
    if (bytes.size() < signature.size()) {
        return false;
    }
    for (std::size_t i = 0; i < signature.size(); ++i) {
        if (bytes[i] != signature[i]) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline RasterFormat detect_raster_format(std::span<const std::uint8_t> bytes) noexcept {
    constexpr std::array<std::uint8_t, 8U> png{{0x89U, 0x50U, 0x4eU, 0x47U, 0x0dU, 0x0aU, 0x1aU, 0x0aU}};
    constexpr std::array<std::uint8_t, 3U> jpeg{{0xffU, 0xd8U, 0xffU}};
    constexpr std::array<std::uint8_t, 4U> tiff_le{{0x49U, 0x49U, 0x2aU, 0x00U}};
    constexpr std::array<std::uint8_t, 4U> tiff_be{{0x4dU, 0x4dU, 0x00U, 0x2aU}};
    constexpr std::array<std::uint8_t, 4U> riff{{0x52U, 0x49U, 0x46U, 0x46U}};
    constexpr std::array<std::uint8_t, 4U> webp{{0x57U, 0x45U, 0x42U, 0x50U}};

    if (raster_signature_starts_with(bytes, png)) {
        return RasterFormat::Png;
    }
    if (raster_signature_starts_with(bytes, jpeg)) {
        return RasterFormat::Jpeg;
    }
    if (raster_signature_starts_with(bytes, tiff_le) || raster_signature_starts_with(bytes, tiff_be)) {
        return RasterFormat::Tiff;
    }
    if (bytes.size() >= 12U && raster_signature_starts_with(bytes, riff) &&
        raster_signature_starts_with(bytes.subspan(8U), webp)) {
        return RasterFormat::Webp;
    }
    return RasterFormat::Unknown;
}

[[nodiscard]] inline RasterInputResult load_raster_input(
    const std::filesystem::path& path,
    std::size_t max_bytes = raster_input_max_bytes) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        return {RasterInputError::OpenFailed, {}};
    }

    const std::streampos end = stream.tellg();
    if (end <= std::streampos{0}) {
        return {RasterInputError::EmptyFile, {}};
    }
    const auto signed_size = static_cast<std::streamoff>(end);
    if (signed_size <= 0) {
        return {RasterInputError::EmptyFile, {}};
    }
    const auto raw_size = static_cast<std::uintmax_t>(signed_size);
    if (raw_size > static_cast<std::uintmax_t>(max_bytes) ||
        raw_size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())) {
        return {RasterInputError::TooLarge, {}};
    }

    const auto size = static_cast<std::size_t>(raw_size);
    std::vector<std::uint8_t> bytes(size);
    stream.seekg(0, std::ios::beg);
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    if (!stream || static_cast<std::size_t>(stream.gcount()) != size) {
        return {RasterInputError::ReadFailed, {}};
    }

    const RasterFormat format = detect_raster_format(bytes);
    if (format == RasterFormat::Unknown) {
        return {RasterInputError::UnsupportedFormat, {}};
    }
    return {RasterInputError::None, RasterInput{format, std::move(bytes)}};
}

[[nodiscard]] inline std::string_view raster_format_name(RasterFormat format) noexcept {
    switch (format) {
        case RasterFormat::Unknown: return "unknown";
        case RasterFormat::Png: return "png";
        case RasterFormat::Jpeg: return "jpeg";
        case RasterFormat::Webp: return "webp";
        case RasterFormat::Tiff: return "tiff";
    }
    return "unknown";
}

[[nodiscard]] inline std::string_view raster_input_error_name(RasterInputError error) noexcept {
    switch (error) {
        case RasterInputError::None: return "none";
        case RasterInputError::OpenFailed: return "open_failed";
        case RasterInputError::EmptyFile: return "empty_file";
        case RasterInputError::TooLarge: return "too_large";
        case RasterInputError::ReadFailed: return "read_failed";
        case RasterInputError::UnsupportedFormat: return "unsupported_format";
    }
    return "unknown";
}

}  // namespace vektoryum::io
