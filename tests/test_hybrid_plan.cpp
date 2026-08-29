#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

#include "vektoryum/hybrid/hybrid_plan.hpp"

namespace {

int failures = 0;

void expect_true(bool condition, std::string_view name) {
    if (!condition) {
        ++failures;
        std::cerr << "[FAIL] " << name << '\n';
        return;
    }
    std::cout << "[PASS] " << name << '\n';
}

[[nodiscard]] vektoryum::hybrid::HybridPlan valid_plan() {
    using namespace vektoryum::hybrid;
    HybridPlan plan;
    plan.schema_version = "1";
    plan.plan_id = "stage9-regression";
    plan.width = 4096U;
    plan.height = 4096U;
    plan.contributions = {
        HybridContribution{"vector-base", ContributionKind::Vector, 0U, 0.35, 1.0},
        HybridContribution{"raster-detail", ContributionKind::Raster, 1U, 0.65, 1.0},
    };
    return plan;
}

}  // namespace

int main() {
    using namespace vektoryum::hybrid;

    const HybridPlan baseline = valid_plan();
    const HybridPlanValidation accepted = validate_hybrid_plan(baseline);
    expect_true(accepted.ok(), "valid deterministic hybrid plan is accepted");
    expect_true(accepted.pixels == 16'777'216ULL, "hybrid pixel accounting is exact");

    HybridPlan invalid = baseline;
    invalid.schema_version.clear();
    expect_true(validate_hybrid_plan(invalid).error == HybridPlanError::MissingSchemaVersion,
                "missing schema version is rejected");

    invalid = baseline;
    invalid.contributions[1].contribution_id = invalid.contributions[0].contribution_id;
    expect_true(validate_hybrid_plan(invalid).error == HybridPlanError::DuplicateContributionIdentity,
                "duplicate contribution identity is rejected");

    invalid = baseline;
    invalid.contributions[1].z_order = 3U;
    expect_true(validate_hybrid_plan(invalid).error == HybridPlanError::NonCanonicalZOrder,
                "noncanonical z ordering is rejected");

    invalid = baseline;
    invalid.contributions[0].coverage = std::numeric_limits<double>::quiet_NaN();
    expect_true(validate_hybrid_plan(invalid).error == HybridPlanError::InvalidCoverage,
                "non-finite coverage is rejected");

    invalid = baseline;
    invalid.contributions[0].opacity = 1.01;
    expect_true(validate_hybrid_plan(invalid).error == HybridPlanError::InvalidOpacity,
                "opacity outside unit interval is rejected");

    invalid = baseline;
    invalid.contributions[1].kind = ContributionKind::Vector;
    expect_true(validate_hybrid_plan(invalid).error == HybridPlanError::MissingRasterContribution,
                "hybrid plan requires raster contribution");

    invalid = baseline;
    invalid.contributions[0].kind = ContributionKind::Raster;
    expect_true(validate_hybrid_plan(invalid).error == HybridPlanError::MissingVectorContribution,
                "hybrid plan requires vector contribution");

    HybridPlanLimits limits;
    limits.max_pixels = 1024U;
    expect_true(validate_hybrid_plan(baseline, limits).error == HybridPlanError::PixelBudgetExceeded,
                "hybrid pixel budget is fail-closed");

    limits = HybridPlanLimits{};
    limits.max_contributions = 1U;
    expect_true(validate_hybrid_plan(baseline, limits).error == HybridPlanError::TooManyContributions,
                "hybrid contribution budget is fail-closed");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All hybrid plan tests passed\n";
    return EXIT_SUCCESS;
}
