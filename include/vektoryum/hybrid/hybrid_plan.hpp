#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vektoryum::hybrid {

enum class ContributionKind : std::uint8_t {
    Vector,
    Raster,
};

struct HybridContribution {
    std::string contribution_id;
    ContributionKind kind{ContributionKind::Raster};
    std::uint32_t z_order{0U};
    double coverage{0.0};
    double opacity{1.0};
};

struct HybridPlan {
    std::string schema_version;
    std::string plan_id;
    std::uint32_t width{0U};
    std::uint32_t height{0U};
    std::vector<HybridContribution> contributions;
};

struct HybridPlanLimits {
    std::uint64_t max_pixels{268'435'456ULL};
    std::size_t max_contributions{4096U};
};

enum class HybridPlanError : std::uint8_t {
    None,
    MissingSchemaVersion,
    MissingPlanIdentity,
    ZeroDimension,
    PixelBudgetExceeded,
    EmptyContributions,
    TooManyContributions,
    MissingContributionIdentity,
    DuplicateContributionIdentity,
    NonCanonicalZOrder,
    InvalidCoverage,
    InvalidOpacity,
    MissingVectorContribution,
    MissingRasterContribution,
};

struct HybridPlanValidation {
    HybridPlanError error{HybridPlanError::None};
    std::size_t contribution_index{0U};
    std::uint64_t pixels{0U};

    [[nodiscard]] bool ok() const noexcept { return error == HybridPlanError::None; }
};

[[nodiscard]] HybridPlanValidation validate_hybrid_plan(
    const HybridPlan& plan, const HybridPlanLimits& limits = {});

}  // namespace vektoryum::hybrid
