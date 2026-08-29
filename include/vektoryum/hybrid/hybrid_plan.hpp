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

enum class RoutingClass : std::uint8_t {
    Geometry,
    PhotographicDetail,
};

struct HybridContribution {
    std::string contribution_id;
    ContributionKind kind{ContributionKind::Raster};
    RoutingClass routing_class{RoutingClass::PhotographicDetail};
    std::string source_id;
    std::string source_revision;
    std::string source_sha256;
    std::uint32_t z_order{0U};
    double coverage{0.0};
    double opacity{1.0};
    std::uint64_t intermediate_bytes{0U};
    std::uint64_t execution_units{0U};
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
    std::uint64_t max_intermediate_bytes{4ULL * 1024ULL * 1024ULL * 1024ULL};
    std::uint64_t max_execution_units{1'000'000'000ULL};
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
    MissingSourceIdentity,
    MissingSourceRevision,
    InvalidSourceDigest,
    RoutingKindMismatch,
    ZeroIntermediateBytes,
    IntermediateByteBudgetExceeded,
    ZeroExecutionUnits,
    ExecutionBudgetExceeded,
    MissingVectorContribution,
    MissingRasterContribution,
};

struct HybridPlanValidation {
    HybridPlanError error{HybridPlanError::None};
    std::size_t contribution_index{0U};
    std::uint64_t pixels{0U};
    std::uint64_t intermediate_bytes{0U};
    std::uint64_t execution_units{0U};

    [[nodiscard]] bool ok() const noexcept { return error == HybridPlanError::None; }
};

[[nodiscard]] HybridPlanValidation validate_hybrid_plan(
    const HybridPlan& plan, const HybridPlanLimits& limits = {});

}  // namespace vektoryum::hybrid
