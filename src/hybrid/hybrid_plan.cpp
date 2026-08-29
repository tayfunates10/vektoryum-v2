#include "vektoryum/hybrid/hybrid_plan.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace vektoryum::hybrid {

namespace {

[[nodiscard]] HybridPlanValidation fail(HybridPlanError error, std::size_t index,
                                        std::uint64_t pixels,
                                        std::uint64_t intermediate_bytes = 0U,
                                        std::uint64_t execution_units = 0U) noexcept {
    return HybridPlanValidation{error, index, pixels, intermediate_bytes, execution_units};
}

[[nodiscard]] bool is_lower_hex_sha256(const std::string& digest) noexcept {
    return digest.size() == 64U &&
           std::all_of(digest.begin(), digest.end(), [](char value) {
               return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
           });
}

[[nodiscard]] bool is_known_contribution_kind(ContributionKind kind) noexcept {
    return kind == ContributionKind::Vector || kind == ContributionKind::Raster;
}

[[nodiscard]] bool route_matches_kind(RoutingClass routing_class,
                                      ContributionKind kind) noexcept {
    return (routing_class == RoutingClass::Geometry && kind == ContributionKind::Vector) ||
           (routing_class == RoutingClass::PhotographicDetail && kind == ContributionKind::Raster);
}

[[nodiscard]] bool add_with_limit(std::uint64_t value, std::uint64_t limit,
                                  std::uint64_t& total) noexcept {
    if (value > limit || total > limit - value) {
        return false;
    }
    total += value;
    return true;
}

}  // namespace

HybridPlanValidation validate_hybrid_plan(const HybridPlan& plan,
                                          const HybridPlanLimits& limits) {
    if (plan.schema_version.empty()) {
        return fail(HybridPlanError::MissingSchemaVersion, 0U, 0U);
    }
    if (plan.plan_id.empty()) {
        return fail(HybridPlanError::MissingPlanIdentity, 0U, 0U);
    }
    if (plan.width == 0U || plan.height == 0U) {
        return fail(HybridPlanError::ZeroDimension, 0U, 0U);
    }

    const std::uint64_t width = plan.width;
    const std::uint64_t height = plan.height;
    if (height > std::numeric_limits<std::uint64_t>::max() / width) {
        return fail(HybridPlanError::PixelBudgetExceeded, 0U, 0U);
    }
    const std::uint64_t pixels = width * height;
    if (pixels > limits.max_pixels) {
        return fail(HybridPlanError::PixelBudgetExceeded, 0U, pixels);
    }
    if (plan.contributions.empty()) {
        return fail(HybridPlanError::EmptyContributions, 0U, pixels);
    }
    if (plan.contributions.size() > limits.max_contributions) {
        return fail(HybridPlanError::TooManyContributions, 0U, pixels);
    }

    std::unordered_set<std::string> ids;
    ids.reserve(plan.contributions.size());
    bool has_vector = false;
    bool has_raster = false;
    std::uint64_t intermediate_bytes = 0U;
    std::uint64_t execution_units = 0U;

    for (std::size_t index = 0U; index < plan.contributions.size(); ++index) {
        const HybridContribution& contribution = plan.contributions[index];
        if (contribution.contribution_id.empty()) {
            return fail(HybridPlanError::MissingContributionIdentity, index, pixels,
                        intermediate_bytes, execution_units);
        }
        if (!ids.insert(contribution.contribution_id).second) {
            return fail(HybridPlanError::DuplicateContributionIdentity, index, pixels,
                        intermediate_bytes, execution_units);
        }
        if (contribution.z_order != index) {
            return fail(HybridPlanError::NonCanonicalZOrder, index, pixels,
                        intermediate_bytes, execution_units);
        }
        if (!std::isfinite(contribution.coverage) || contribution.coverage <= 0.0 ||
            contribution.coverage > 1.0) {
            return fail(HybridPlanError::InvalidCoverage, index, pixels,
                        intermediate_bytes, execution_units);
        }
        if (!std::isfinite(contribution.opacity) || contribution.opacity < 0.0 ||
            contribution.opacity > 1.0) {
            return fail(HybridPlanError::InvalidOpacity, index, pixels,
                        intermediate_bytes, execution_units);
        }
        if (contribution.source_id.empty()) {
            return fail(HybridPlanError::MissingSourceIdentity, index, pixels,
                        intermediate_bytes, execution_units);
        }
        if (contribution.source_revision.empty()) {
            return fail(HybridPlanError::MissingSourceRevision, index, pixels,
                        intermediate_bytes, execution_units);
        }
        if (!is_lower_hex_sha256(contribution.source_sha256)) {
            return fail(HybridPlanError::InvalidSourceDigest, index, pixels,
                        intermediate_bytes, execution_units);
        }
        if (!is_known_contribution_kind(contribution.kind)) {
            return fail(HybridPlanError::RoutingKindMismatch, index, pixels,
                        intermediate_bytes, execution_units);
        }
        if (!route_matches_kind(contribution.routing_class, contribution.kind)) {
            return fail(HybridPlanError::RoutingKindMismatch, index, pixels,
                        intermediate_bytes, execution_units);
        }
        if (contribution.intermediate_bytes == 0U) {
            return fail(HybridPlanError::ZeroIntermediateBytes, index, pixels,
                        intermediate_bytes, execution_units);
        }
        if (!add_with_limit(contribution.intermediate_bytes, limits.max_intermediate_bytes,
                            intermediate_bytes)) {
            return fail(HybridPlanError::IntermediateByteBudgetExceeded, index, pixels,
                        intermediate_bytes, execution_units);
        }
        if (contribution.execution_units == 0U) {
            return fail(HybridPlanError::ZeroExecutionUnits, index, pixels,
                        intermediate_bytes, execution_units);
        }
        if (!add_with_limit(contribution.execution_units, limits.max_execution_units,
                            execution_units)) {
            return fail(HybridPlanError::ExecutionBudgetExceeded, index, pixels,
                        intermediate_bytes, execution_units);
        }
        has_vector = has_vector || contribution.kind == ContributionKind::Vector;
        has_raster = has_raster || contribution.kind == ContributionKind::Raster;
    }

    if (!has_vector) {
        return fail(HybridPlanError::MissingVectorContribution, plan.contributions.size(), pixels,
                    intermediate_bytes, execution_units);
    }
    if (!has_raster) {
        return fail(HybridPlanError::MissingRasterContribution, plan.contributions.size(), pixels,
                    intermediate_bytes, execution_units);
    }

    return HybridPlanValidation{HybridPlanError::None, plan.contributions.size(), pixels,
                                intermediate_bytes, execution_units};
}

}  // namespace vektoryum::hybrid
