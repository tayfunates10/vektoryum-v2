#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "vektoryum/hybrid/hybrid_output_contract.hpp"

namespace vektoryum::exporting {

enum class ExportFormat : std::uint8_t {
    Svg,
    Pdf,
    Eps,
    Dxf,
};

struct ExportRequest {
    std::string schema_version;
    std::string export_id;
    ExportFormat format{ExportFormat::Svg};
    std::string source_output_id;
    std::string source_output_sha256;
    std::uint32_t width{0U};
    std::uint32_t height{0U};
    std::uint64_t estimated_output_bytes{0U};
};

struct ExportLimits {
    std::uint64_t max_pixels{268'435'456ULL};
    std::uint64_t max_output_bytes{512ULL * 1024ULL * 1024ULL};
};

enum class ExportRequestError : std::uint8_t {
    None,
    MissingSchemaVersion,
    MissingExportIdentity,
    UnsafeTextField,
    UnsupportedFormat,
    MissingSourceOutputIdentity,
    InvalidSourceOutputDigest,
    SourceOutputIdentityMismatch,
    SourceOutputDigestMismatch,
    ZeroDimension,
    PixelBudgetExceeded,
    ZeroEstimatedOutputBytes,
    OutputBudgetExceeded,
};

struct ExportRequestValidation {
    ExportRequestError error{ExportRequestError::None};
    std::uint64_t pixels{0U};

    [[nodiscard]] bool ok() const noexcept { return error == ExportRequestError::None; }
};

[[nodiscard]] ExportRequestValidation validate_export_request(
    const ExportRequest& request,
    const hybrid::HybridOutputManifest& source_output,
    const ExportLimits& limits = {});

[[nodiscard]] std::string canonical_export_request_report(const ExportRequest& request);

}  // namespace vektoryum::exporting
