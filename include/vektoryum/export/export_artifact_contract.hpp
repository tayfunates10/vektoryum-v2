#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "vektoryum/export/export_contract.hpp"

namespace vektoryum::exporting {

struct ExportExecutionLimits {
    std::uint64_t max_intermediate_bytes{256ULL * 1024ULL * 1024ULL};
    std::uint64_t max_execution_units{1'000'000ULL};
};

struct EncodedExportArtifact {
    ExportFormat format{ExportFormat::Svg};
    std::string export_id;
    std::string source_output_id;
    std::string source_output_sha256;
    std::string output_sha256;
    std::vector<std::uint8_t> bytes;
    std::uint64_t peak_intermediate_bytes{0U};
    std::uint64_t execution_units{0U};
};

enum class ExportArtifactError : std::uint8_t {
    None,
    InvalidRequest,
    FormatMismatch,
    ExportIdentityMismatch,
    SourceOutputIdentityMismatch,
    SourceOutputDigestMismatch,
    EmptyOutput,
    OutputBudgetExceeded,
    InvalidOutputDigest,
    OutputDigestMismatch,
    ZeroIntermediateBytes,
    IntermediateBudgetExceeded,
    ZeroExecutionUnits,
    ExecutionBudgetExceeded,
    InvalidStructure,
};

struct ExportArtifactValidation {
    ExportArtifactError error{ExportArtifactError::None};

    [[nodiscard]] bool ok() const noexcept { return error == ExportArtifactError::None; }
};

[[nodiscard]] ExportArtifactValidation validate_encoded_export_artifact(
    const ExportRequest& request,
    const hybrid::HybridOutputManifest& source_output,
    const EncodedExportArtifact& artifact,
    const ExportLimits& request_limits = {},
    const ExportExecutionLimits& execution_limits = {});

[[nodiscard]] std::string canonical_export_artifact_report(const EncodedExportArtifact& artifact);

}  // namespace vektoryum::exporting
