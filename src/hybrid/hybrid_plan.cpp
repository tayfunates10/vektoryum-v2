#include "vektoryum/hybrid/hybrid_plan.hpp"

#include <cmath>
#include <limits>
#include <unordered_set>

namespace vektoryum::hybrid {

namespace {

[[nodiscard]] HybridPlanValidation fail(HybridPlanError error, std::size_t index,
                                        std::uint64_t pixels) noexcept {
    return HybridPlanValidation{error, index, pixels};
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

    for (std::size_t index = 0U; index < plan.contributions.size(); ++index) {
        const HybridContribution& contribution = plan.contributions[index];
        if (contribution.contribution_id.empty()) {
            return fail(HybridPlanError::MissingContributionIdentity, index, pixels);
        }
        if (!ids.insert(contribution.contribution_id).second) {
            return fail(HybridPlanError::DuplicateContributionIdentity, index, pixels);
        }
        if (contribution.z_order != index) {
            return fail(HybridPlanError::NonCanonicalZOrder, index, pixels);
        }
        if (!std::isfinite(contribution.coverage) || contribution.coverage <= 0.0 ||
            contribution.coverage > 1.0) {
            return fail(HybridPlanError::InvalidCoverage, index, pixels);
        }
        if (!std::isfinite(contribution.opacity) || contribution.opacity < 0.0 ||
            contribution.opacity > 1.0) {
            return fail(HybridPlanError::InvalidOpacity, index, pixels);
        }
        has_vector = has_vector || contribution.kind == ContributionKind::Vector;
        has_raster = has_raster || contribution.kind == ContributionKind::Raster;
    }

    if (!has_vector) {
        return fail(HybridPlanError::MissingVectorContribution, plan.contributions.size(), pixels);
    }
    if (!has_raster) {
        return fail(HybridPlanError::MissingRasterContribution, plan.contributions.size(), pixels);
    }

    return HybridPlanValidation{HybridPlanError::None, plan.contributions.size(), pixels};
}

}  // namespace vektoryum::hybrid
