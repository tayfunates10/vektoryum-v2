#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include "vektoryum/hybrid/alpha_composite.hpp"
#include "vektoryum/hybrid/hybrid_output_contract.hpp"
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

[[nodiscard]] bool same_sample(const vektoryum::hybrid::RgbaSample& lhs,
                               const vektoryum::hybrid::RgbaSample& rhs) {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

[[nodiscard]] vektoryum::hybrid::HybridPlan mixed_plan() {
    using namespace vektoryum::hybrid;
    HybridPlan plan;
    plan.schema_version = "2";
    plan.plan_id = "stage9-mixed-content-e2e";
    plan.width = 4U;
    plan.height = 1U;
    plan.contributions = {
        HybridContribution{"vector-geometry", ContributionKind::Vector, RoutingClass::Geometry,
                           "tiny-vector-feature", "vector-r18",
                           "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
                           0U, 0.25, 1.0, 256U, 32U},
        HybridContribution{"raster-detail", ContributionKind::Raster,
                           RoutingClass::PhotographicDetail, "high-frequency-raster", "raster-r10",
                           "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789",
                           1U, 0.75, 1.0, 512U, 64U},
    };
    return plan;
}

[[nodiscard]] vektoryum::hybrid::HybridOutputManifest output_for(
    const vektoryum::hybrid::HybridPlan& plan) {
    using namespace vektoryum::hybrid;
    HybridOutputManifest output;
    output.schema_version = "1";
    output.output_id = "mixed-output-001";
    output.output_sha256 =
        "2222222222222222222222222222222222222222222222222222222222222222";
    output.plan_id = plan.plan_id;
    output.vector_topology_revision = "topology-r5";
    output.seam_error = 0.004;
    output.expected_vector_components = 1U;
    output.actual_vector_components = 1U;
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

    const HybridPlan plan = mixed_plan();
    expect_true(validate_hybrid_plan(plan).ok(),
                "mixed vector/raster execution plan satisfies deterministic routing contract");

    // Four representative pixels exercise a tiny vector feature, two overlapping
    // vector/raster seam pixels, and alternating high-frequency raster detail.
    const std::array<std::array<RgbaSample, 2>, 4> layers = {{
        {{{0.10, 0.10, 0.10, 1.0}, {0.90, 0.90, 0.90, 0.0}}},
        {{{0.20, 0.20, 0.20, 1.0}, {0.80, 0.20, 0.20, 0.5}}},
        {{{0.80, 0.80, 0.80, 1.0}, {0.20, 0.80, 0.20, 0.5}}},
        {{{0.15, 0.15, 0.15, 1.0}, {0.85, 0.85, 0.85, 0.0}}},
    }};

    std::array<RgbaSample, 4> first_pass{};
    std::array<RgbaSample, 4> second_pass{};
    bool all_ok = true;
    for (std::size_t index = 0U; index < layers.size(); ++index) {
        const AlphaCompositeResult a = composite_alpha_safe(layers[index]);
        const AlphaCompositeResult b = composite_alpha_safe(layers[index]);
        all_ok = all_ok && a.ok() && b.ok();
        first_pass[index] = a.output;
        second_pass[index] = b.output;
    }
    expect_true(all_ok, "mixed-content alpha execution succeeds without hidden-RGB leakage");

    bool deterministic = true;
    for (std::size_t index = 0U; index < first_pass.size(); ++index) {
        deterministic = deterministic && same_sample(first_pass[index], second_pass[index]);
    }
    expect_true(deterministic,
                "overlap, tiny-vector and high-frequency raster execution is deterministic");
    expect_true(first_pass[1].r != first_pass[2].r || first_pass[1].g != first_pass[2].g,
                "high-frequency raster detail remains distinguishable across adjacent seam pixels");
    expect_true(first_pass[0].r == 0.10 && first_pass[0].a == 1.0,
                "tiny vector-owned feature survives transparent raster coverage without fallback");

    const HybridOutputManifest output = output_for(plan);
    expect_true(validate_hybrid_output(plan, output).ok(),
                "mixed-content final output satisfies seam, topology and provenance contracts");
    const std::string report_a = canonical_hybrid_output_report(plan, output);
    const std::string report_b = canonical_hybrid_output_report(plan, output);
    expect_true(report_a == report_b,
                "end-to-end mixed-content provenance evidence is canonical and deterministic");
    expect_true(report_a.find("tiny-vector-feature") != std::string::npos &&
                    report_a.find("high-frequency-raster") != std::string::npos,
                "end-to-end evidence records both vector and raster source identities");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All hybrid mixed-content execution tests passed\n";
    return EXIT_SUCCESS;
}
