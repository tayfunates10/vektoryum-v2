#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "vektoryum/vector/reconstruction.hpp"

namespace vektoryum::vector {

enum class PathQualityError : std::uint8_t {
    None,
    InvalidScene,
    SelfIntersection,
    CertificationBudgetExceeded,
    FidelityRejected,
};

struct PathQualityOptions {
    double min_iou{0.995};
    double max_disagreement_ratio{0.005};
    std::size_t max_nodes{1'000'000U};
    std::uint64_t max_certification_pixels{16'777'216U};
};

struct PathQualityReport {
    PathQualityError error{PathQualityError::None};
    std::size_t total_nodes{};
    std::size_t self_intersections{};
    double raster_iou{};
    double disagreement_ratio{};

    [[nodiscard]] bool passed() const noexcept { return error == PathQualityError::None; }
};

[[nodiscard]] bool path_has_self_intersection(const Path& path) noexcept;

[[nodiscard]] PathQualityReport certify_scene(
    const VectorScene& scene,
    std::span<const std::uint8_t> reference_mask,
    std::uint32_t width,
    std::uint32_t height,
    PathQualityOptions options = {});

struct SimplificationResult {
    PathQualityError error{PathQualityError::None};
    VectorScene scene{};
    PathQualityReport quality{};
    std::size_t removed_nodes{};

    [[nodiscard]] bool ok() const noexcept { return error == PathQualityError::None; }
};

[[nodiscard]] SimplificationResult simplify_scene_fidelity_gated(
    const VectorScene& input,
    std::span<const std::uint8_t> reference_mask,
    std::uint32_t width,
    std::uint32_t height,
    PathQualityOptions options = {});

}  // namespace vektoryum::vector
