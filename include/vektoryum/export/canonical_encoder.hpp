#pragma once

#include "vektoryum/export/export_artifact_contract.hpp"
#include "vektoryum/vector/svg_path.hpp"

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

// R5 geometry-backed export path. This additive entry point preserves the
// existing canonical API while requiring actual reconstructed scene geometry.
[[nodiscard]] CanonicalEncodeResult encode_geometry_export(
    const ExportRequest& request,
    const hybrid::HybridOutputManifest& source_output,
    const vector::SvgScene& scene,
    const ExportLimits& request_limits = {},
    const ExportExecutionLimits& execution_limits = {});

}  // namespace vektoryum::exporting
