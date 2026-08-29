#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vektoryum/hybrid/hybrid_plan.hpp"

namespace vektoryum::hybrid {

struct HybridOutputContribution {
    std::string contribution_id;
    std::string source_revision;
    std::string source_sha256;
};

struct HybridOutputManifest {
    std::string schema_version;
    std::string output_id;
    std::string output_sha256;
    std::string plan_id;
    std::string vector_topology_revision;
    double seam_error{0.0};
    std::uint64_t expected_vector_components{0U};
    std::uint64_t actual_vector_components{0U};
    bool raster_fallback_replaced_vector{false};
    std::vector<HybridOutputContribution> contributions;
};

struct HybridOutputLimits {
    double max_seam_error{0.01};
    std::size_t max_contributions{4096U};
};

enum class HybridOutputError : std::uint8_t {
    None,
    MissingSchemaVersion,
    MissingOutputIdentity,
    InvalidOutputDigest,
    PlanIdentityMismatch,
    MissingTopologyRevision,
    InvalidSeamError,
    SeamThresholdExceeded,
    InvalidTopologyExpectation,
    TopologyMismatch,
    SilentRasterFallback,
    ContributionCountMismatch,
    TooManyContributions,
    ContributionIdentityMismatch,
    ContributionRevisionMismatch,
    ContributionDigestMismatch,
};

struct HybridOutputValidation {
    HybridOutputError error{HybridOutputError::None};
    std::size_t contribution_index{0U};

    [[nodiscard]] bool ok() const noexcept { return error == HybridOutputError::None; }
};

[[nodiscard]] HybridOutputValidation validate_hybrid_output(
    const HybridPlan& plan, const HybridOutputManifest& output,
    const HybridOutputLimits& limits = {});

[[nodiscard]] std::string canonical_hybrid_output_report(
    const HybridPlan& plan, const HybridOutputManifest& output);

}  // namespace vektoryum::hybrid
