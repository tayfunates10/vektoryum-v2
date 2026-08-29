#include "vektoryum/hybrid/hybrid_output_contract.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace vektoryum::hybrid {

namespace {

[[nodiscard]] bool is_lower_hex_sha256(const std::string& digest) noexcept {
    return digest.size() == 64U &&
           std::all_of(digest.begin(), digest.end(), [](char value) {
               return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
           });
}

[[nodiscard]] HybridOutputValidation fail(HybridOutputError error,
                                          std::size_t index = 0U) noexcept {
    return HybridOutputValidation{error, index};
}

}  // namespace

HybridOutputValidation validate_hybrid_output(const HybridPlan& plan,
                                              const HybridOutputManifest& output,
                                              const HybridOutputLimits& limits) {
    if (output.schema_version.empty()) {
        return fail(HybridOutputError::MissingSchemaVersion);
    }
    if (output.output_id.empty()) {
        return fail(HybridOutputError::MissingOutputIdentity);
    }
    if (!is_lower_hex_sha256(output.output_sha256)) {
        return fail(HybridOutputError::InvalidOutputDigest);
    }
    if (output.plan_id != plan.plan_id || output.plan_id.empty()) {
        return fail(HybridOutputError::PlanIdentityMismatch);
    }
    if (output.vector_topology_revision.empty()) {
        return fail(HybridOutputError::MissingTopologyRevision);
    }
    if (!std::isfinite(output.seam_error) || output.seam_error < 0.0 ||
        !std::isfinite(limits.max_seam_error) || limits.max_seam_error < 0.0) {
        return fail(HybridOutputError::InvalidSeamError);
    }
    if (output.seam_error > limits.max_seam_error) {
        return fail(HybridOutputError::SeamThresholdExceeded);
    }
    if (output.expected_vector_components == 0U) {
        return fail(HybridOutputError::InvalidTopologyExpectation);
    }
    if (output.expected_vector_components != output.actual_vector_components) {
        return fail(HybridOutputError::TopologyMismatch);
    }
    if (output.raster_fallback_replaced_vector) {
        return fail(HybridOutputError::SilentRasterFallback);
    }
    if (output.contributions.size() > limits.max_contributions) {
        return fail(HybridOutputError::TooManyContributions);
    }
    if (output.contributions.size() != plan.contributions.size()) {
        return fail(HybridOutputError::ContributionCountMismatch);
    }

    for (std::size_t index = 0U; index < plan.contributions.size(); ++index) {
        const HybridContribution& planned = plan.contributions[index];
        const HybridOutputContribution& actual = output.contributions[index];
        if (actual.contribution_id != planned.contribution_id) {
            return fail(HybridOutputError::ContributionIdentityMismatch, index);
        }
        if (actual.source_revision != planned.source_revision) {
            return fail(HybridOutputError::ContributionRevisionMismatch, index);
        }
        if (actual.source_sha256 != planned.source_sha256 ||
            !is_lower_hex_sha256(actual.source_sha256)) {
            return fail(HybridOutputError::ContributionDigestMismatch, index);
        }
    }

    return HybridOutputValidation{};
}

std::string canonical_hybrid_output_report(const HybridPlan& plan,
                                           const HybridOutputManifest& output) {
    std::ostringstream report;
    report << "schema_version=" << output.schema_version << '\n';
    report << "output_id=" << output.output_id << '\n';
    report << "output_sha256=" << output.output_sha256 << '\n';
    report << "plan_id=" << output.plan_id << '\n';
    report << "vector_topology_revision=" << output.vector_topology_revision << '\n';
    report << "expected_vector_components=" << output.expected_vector_components << '\n';
    report << "actual_vector_components=" << output.actual_vector_components << '\n';
    report << "raster_fallback_replaced_vector="
           << (output.raster_fallback_replaced_vector ? "true" : "false") << '\n';
    report << "contribution_count=" << output.contributions.size() << '\n';
    for (std::size_t index = 0U; index < output.contributions.size(); ++index) {
        const HybridOutputContribution& contribution = output.contributions[index];
        report << "contribution[" << index << "].id=" << contribution.contribution_id << '\n';
        report << "contribution[" << index << "].revision=" << contribution.source_revision << '\n';
        report << "contribution[" << index << "].sha256=" << contribution.source_sha256 << '\n';
    }
    report << "plan_contribution_count=" << plan.contributions.size() << '\n';
    return report.str();
}

}  // namespace vektoryum::hybrid
