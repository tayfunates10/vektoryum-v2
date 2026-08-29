#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

#include "vektoryum/hybrid/hybrid_output_contract.hpp"

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
    plan.schema_version = "2";
    plan.plan_id = "stage9-output-contract";
    plan.width = 1024U;
    plan.height = 1024U;
    plan.contributions = {
        HybridContribution{"vector-base", ContributionKind::Vector, RoutingClass::Geometry,
                           "geometry", "vector-r17",
                           "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
                           0U, 0.5, 1.0, 1024U, 100U},
        HybridContribution{"raster-detail", ContributionKind::Raster,
                           RoutingClass::PhotographicDetail, "photo", "raster-r9",
                           "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789",
                           1U, 0.5, 1.0, 2048U, 200U},
    };
    return plan;
}

[[nodiscard]] vektoryum::hybrid::HybridOutputManifest valid_output(
    const vektoryum::hybrid::HybridPlan& plan) {
    using namespace vektoryum::hybrid;
    HybridOutputManifest output;
    output.schema_version = "1";
    output.output_id = "hybrid-output-001";
    output.output_sha256 =
        "1111111111111111111111111111111111111111111111111111111111111111";
    output.plan_id = plan.plan_id;
    output.vector_topology_revision = "topology-r4";
    output.seam_error = 0.005;
    output.expected_vector_components = 3U;
    output.actual_vector_components = 3U;
    output.contributions = {
        HybridOutputContribution{plan.contributions[0].contribution_id,
                                 plan.contributions[0].source_revision,
                                 plan.contributions[0].source_sha256},
        HybridOutputContribution{plan.contributions[1].contribution_id,
                                 plan.contributions[1].source_revision,
                                 plan.contributions[1].source_sha256},
    };
    return output;
}

}  // namespace

int main() {
    using namespace vektoryum::hybrid;

    const HybridPlan plan = valid_plan();
    const HybridOutputManifest baseline = valid_output(plan);
    expect_true(validate_hybrid_output(plan, baseline).ok(),
                "valid hybrid output safety evidence is accepted");

    const std::string report_a = canonical_hybrid_output_report(plan, baseline);
    const std::string report_b = canonical_hybrid_output_report(plan, baseline);
    expect_true(report_a == report_b, "hybrid output provenance report is deterministic");
    expect_true(report_a.find("vector-r17") != std::string::npos &&
                    report_a.find("raster-r9") != std::string::npos,
                "provenance report records all source revisions");

    HybridOutputManifest invalid = baseline;
    invalid.output_sha256[0] = 'A';
    expect_true(validate_hybrid_output(plan, invalid).error == HybridOutputError::InvalidOutputDigest,
                "noncanonical output digest is rejected");

    invalid = baseline;
    invalid.plan_id = "other-plan";
    expect_true(validate_hybrid_output(plan, invalid).error == HybridOutputError::PlanIdentityMismatch,
                "output cannot detach from its reconstruction plan");

    invalid = baseline;
    invalid.seam_error = 0.011;
    expect_true(validate_hybrid_output(plan, invalid).error == HybridOutputError::SeamThresholdExceeded,
                "seam threshold cannot be weakened");

    invalid = baseline;
    invalid.seam_error = std::numeric_limits<double>::quiet_NaN();
    expect_true(validate_hybrid_output(plan, invalid).error == HybridOutputError::InvalidSeamError,
                "non-finite seam evidence is rejected");

    invalid = baseline;
    invalid.actual_vector_components = 2U;
    expect_true(validate_hybrid_output(plan, invalid).error == HybridOutputError::TopologyMismatch,
                "vector topology loss is rejected");

    invalid = baseline;
    invalid.raster_fallback_replaced_vector = true;
    expect_true(validate_hybrid_output(plan, invalid).error == HybridOutputError::SilentRasterFallback,
                "silent raster replacement of vector geometry is rejected");

    invalid = baseline;
    invalid.contributions[0].contribution_id = "substituted-vector";
    expect_true(validate_hybrid_output(plan, invalid).error ==
                    HybridOutputError::ContributionIdentityMismatch,
                "output contribution substitution is rejected");

    invalid = baseline;
    invalid.contributions[1].source_revision = "raster-r10";
    expect_true(validate_hybrid_output(plan, invalid).error ==
                    HybridOutputError::ContributionRevisionMismatch,
                "output source revision drift is rejected");

    invalid = baseline;
    invalid.contributions[1].source_sha256[0] = '0';
    expect_true(validate_hybrid_output(plan, invalid).error ==
                    HybridOutputError::ContributionDigestMismatch,
                "output source digest drift is rejected");

    invalid = baseline;
    invalid.contributions.pop_back();
    expect_true(validate_hybrid_output(plan, invalid).error ==
                    HybridOutputError::ContributionCountMismatch,
                "missing output contribution provenance is rejected");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All hybrid output contract tests passed\n";
    return EXIT_SUCCESS;
}
