#include "vektoryum/export/export_contract.hpp"

#include <cassert>
#include <cstdint>
#include <string>

using vektoryum::exporting::ExportFormat;
using vektoryum::exporting::ExportLimits;
using vektoryum::exporting::ExportRequest;
using vektoryum::exporting::ExportRequestError;
using vektoryum::exporting::canonical_export_request_report;
using vektoryum::exporting::validate_export_request;
using vektoryum::hybrid::HybridOutputManifest;

namespace {

[[nodiscard]] ExportRequest valid_request() {
    return ExportRequest{
        "vektoryum.export.v1",
        "export-0001",
        ExportFormat::Svg,
        "hybrid-output-0001",
        std::string(64U, 'a'),
        1024U,
        768U,
        4096U,
    };
}

[[nodiscard]] HybridOutputManifest valid_source_output() {
    HybridOutputManifest output{};
    output.output_id = "hybrid-output-0001";
    output.output_sha256 = std::string(64U, 'a');
    return output;
}

}  // namespace

int main() {
    const ExportRequest baseline = valid_request();
    const HybridOutputManifest source_output = valid_source_output();
    const auto baseline_validation = validate_export_request(baseline, source_output);
    assert(baseline_validation.ok());
    assert(baseline_validation.pixels == 786432U);

    ExportRequest invalid = baseline;
    invalid.format = static_cast<ExportFormat>(255);
    assert(validate_export_request(invalid, source_output).error == ExportRequestError::UnsupportedFormat);

    invalid = baseline;
    invalid.source_output_sha256 = std::string(64U, 'A');
    assert(validate_export_request(invalid, source_output).error == ExportRequestError::InvalidSourceOutputDigest);

    invalid = baseline;
    invalid.source_output_id = "stale-output";
    assert(validate_export_request(invalid, source_output).error == ExportRequestError::SourceOutputIdentityMismatch);

    invalid = baseline;
    invalid.source_output_sha256 = std::string(64U, 'b');
    assert(validate_export_request(invalid, source_output).error == ExportRequestError::SourceOutputDigestMismatch);

    invalid = baseline;
    invalid.schema_version = "vektoryum.export.v1\nexport_id=collision";
    assert(validate_export_request(invalid, source_output).error == ExportRequestError::UnsafeTextField);

    invalid = baseline;
    invalid.export_id = "export-0001\rsource_output_id=collision";
    assert(validate_export_request(invalid, source_output).error == ExportRequestError::UnsafeTextField);

    invalid = baseline;
    invalid.source_output_id = "hybrid-output-0001\nwidth=1";
    assert(validate_export_request(invalid, source_output).error == ExportRequestError::UnsafeTextField);

    invalid = baseline;
    invalid.width = 0U;
    assert(validate_export_request(invalid, source_output).error == ExportRequestError::ZeroDimension);

    invalid = baseline;
    invalid.estimated_output_bytes = 0U;
    assert(validate_export_request(invalid, source_output).error == ExportRequestError::ZeroEstimatedOutputBytes);

    invalid = baseline;
    invalid.estimated_output_bytes = 2049U;
    ExportLimits tight_limits{};
    tight_limits.max_output_bytes = 2048U;
    assert(validate_export_request(invalid, source_output, tight_limits).error == ExportRequestError::OutputBudgetExceeded);

    invalid = baseline;
    invalid.width = 100U;
    invalid.height = 100U;
    tight_limits = ExportLimits{};
    tight_limits.max_pixels = 9999U;
    assert(validate_export_request(invalid, source_output, tight_limits).error == ExportRequestError::PixelBudgetExceeded);

    const std::string report_a = canonical_export_request_report(baseline);
    const std::string report_b = canonical_export_request_report(baseline);
    assert(report_a == report_b);
    assert(report_a.find("format=svg\n") != std::string::npos);
    assert(report_a.find("source_output_id=hybrid-output-0001\n") != std::string::npos);
    assert(report_a.find("source_output_sha256=" + std::string(64U, 'a') + "\n") != std::string::npos);

    return 0;
}
