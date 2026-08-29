#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "vektoryum/vector/reconstruction.hpp"

namespace vektoryum::vector {

struct DoublePoint {
    double x{};
    double y{};

    [[nodiscard]] friend constexpr bool operator==(const DoublePoint&, const DoublePoint&) noexcept = default;
};

enum class SvgCommandType : std::uint8_t {
    MoveTo,
    LineTo,
    CubicTo,
    Close,
};

struct SvgCommand {
    SvgCommandType type{SvgCommandType::Close};
    DoublePoint control1{};
    DoublePoint control2{};
    DoublePoint end{};
};

struct SvgPath {
    std::vector<SvgCommand> commands{};
    bool hole{false};
};

struct SvgScene {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<SvgPath> paths{};
};

enum class SvgFitError : std::uint8_t {
    None,
    InvalidScene,
    DegeneratePath,
    InvalidRadius,
};

struct SvgFitOptions {
    // Zero preserves the polygon exactly. Positive values create deterministic
    // cubic corner candidates that must still pass rasterize-back certification
    // before replacing the certified polygon in a production pipeline.
    double corner_radius{0.0};
};

struct SvgFitResult {
    SvgFitError error{SvgFitError::None};
    SvgScene scene{};

    [[nodiscard]] bool ok() const noexcept { return error == SvgFitError::None; }
};

[[nodiscard]] SvgFitResult fit_svg_paths(
    const VectorScene& scene,
    SvgFitOptions options = {});

// Deterministic path-data serialization suitable for embedding in an SVG path
// element. Coordinates are emitted with fixed six-decimal precision.
[[nodiscard]] std::string serialize_svg_path_data(const SvgPath& path);

}  // namespace vektoryum::vector
