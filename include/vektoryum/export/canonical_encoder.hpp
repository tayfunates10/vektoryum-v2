#pragma once

#include "vektoryum/export/export_artifact_contract.hpp"

namespace vektoryum::exporting {

struct CanonicalEncodeResult {
    ExportArtifactError error{ExportArtifactError::None};
    EncodedExportArtifact artifact;

    [[nodiscard]] bool ok() const noexcept { return error == ExportArtifactError::None; }
};

[[nodiscard]] CanonicalEncodeResult encode_canonical_export(
    const ExportRequest& request,
    const hybrid::HybridOutputManifest& source_output,
    const ExportLimits& request_limits = {},
    const ExportExecutionLimits& execution_limits = {});

}  // namespace vektoryum::exporting
