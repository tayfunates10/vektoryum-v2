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
    constexpr std::string_view digest_a =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    constexpr std::string_view digest_b =
        "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";

    HybridPlan plan;
    plan.schema_version = "2";
    plan.plan_id = "stage9-routing-regression";
    plan.width = 4096U;
    plan.height = 4096U;
    plan.contributions = {
        HybridContribution{"vector-base", ContributionKind::Vector, RoutingClass::Geometry,
                           "geometry-source", "vector-r17", std::string(digest_a), 0U, 0.35, 1.0,
                           64ULL * 1024ULL * 1024ULL, 100'000U},
        HybridContribution{"raster-detail", ContributionKind::Raster,
                           RoutingClass::PhotographicDetail, "photo-source", "raster-r9",
                           std::string(digest_b), 1U, 0.65, 1.0,
                           128ULL * 1024ULL * 1024ULL, 250'000U},
    };
    return plan;
}

}  // namespace

int main() {
    using namespace vektoryum::hybrid;

    const HybridPlan baseline = valid_plan();
    const HybridPlanValidation accepted = validate_hybrid_plan(baseline);
    expect_true(accepted.ok(), "valid deterministic hybrid routing plan is accepted");
    expect_true(accepted.pixels == 16'777'216ULL, "hybrid pixel accounting is exact");
    expect_true(accepted.intermediate_bytes == 192ULL * 1024ULL * 1024ULL,
                "intermediate byte accounting is exact");
    expect_true(accepted.execution_units == 350'000U, "execution accounting is exact");

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
    invalid.contributions[0].source_id.clear();
    expect_true(validate_hybrid_plan(invalid).error == HybridPlanError::MissingSourceIdentity,
                "missing contribution source identity is rejected");

    invalid = baseline;
    invalid.contributions[0].source_revision.clear();
    expect_true(validate_hybrid_plan(invalid).error == HybridPlanError::MissingSourceRevision,
                "missing contribution source revision is rejected");

    invalid = baseline;
    invalid.contributions[0].source_sha256[0] = 'A';
    expect_true(validate_hybrid_plan(invalid).error == HybridPlanError::InvalidSourceDigest,
                "noncanonical source digest is rejected");

    invalid = baseline;
    invalid.contributions[0].routing_class = RoutingClass::PhotographicDetail;
    expect_true(validate_hybrid_plan(invalid).error == HybridPlanError::RoutingKindMismatch,
                "geometry routed to raster class is rejected");

    invalid = baseline;
    invalid.contributions[1].routing_class = RoutingClass::Geometry;
    expect_true(validate_hybrid_plan(invalid).error == HybridPlanError::RoutingKindMismatch,
                "photographic detail routed to vector class is rejected");

    invalid = baseline;
    invalid.contributions[0].intermediate_bytes = 0U;
    expect_true(validate_hybrid_plan(invalid).error == HybridPlanError::ZeroIntermediateBytes,
                "zero intermediate byte declaration is rejected");

    invalid = baseline;
    invalid.contributions[0].execution_units = 0U;
    expect_true(validate_hybrid_plan(invalid).error == HybridPlanError::ZeroExecutionUnits,
                "zero execution budget declaration is rejected");

    invalid = baseline;
    invalid.contributions[1].kind = ContributionKind::Vector;
    invalid.contributions[1].routing_class = RoutingClass::Geometry;
    expect_true(validate_hybrid_plan(invalid).error == HybridPlanError::MissingRasterContribution,
                "hybrid plan requires raster contribution");

    invalid = baseline;
    invalid.contributions[0].kind = ContributionKind::Raster;
    invalid.contributions[0].routing_class = RoutingClass::PhotographicDetail;
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

    limits = HybridPlanLimits{};
    limits.max_intermediate_bytes = 128ULL * 1024ULL * 1024ULL;
    expect_true(validate_hybrid_plan(baseline, limits).error ==
                    HybridPlanError::IntermediateByteBudgetExceeded,
                "aggregate intermediate memory budget is fail-closed");

    limits = HybridPlanLimits{};
    limits.max_execution_units = 300'000U;
    expect_true(validate_hybrid_plan(baseline, limits).error == HybridPlanError::ExecutionBudgetExceeded,
                "aggregate execution budget is fail-closed");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All hybrid plan tests passed\n";
    return EXIT_SUCCESS;
}
