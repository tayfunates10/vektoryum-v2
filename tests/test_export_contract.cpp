#include "vektoryum/export/export_contract.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

using vektoryum::exporting::ExportFormat;
using vektoryum::exporting::ExportLimits;
using vektoryum::exporting::ExportRequest;
using vektoryum::exporting::ExportRequestError;
using vektoryum::exporting::canonical_export_request_report;
using vektoryum::exporting::validate_export_request;
using vektoryum::hybrid::HybridOutputManifest;

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

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
    require(baseline_validation.ok(), "baseline request must validate");
    require(baseline_validation.pixels == 786432U, "baseline pixel count must be exact");

    ExportRequest invalid = baseline;
    invalid.format = static_cast<ExportFormat>(255);
    require(validate_export_request(invalid, source_output).error == ExportRequestError::UnsupportedFormat, "unsupported format must fail closed");

    invalid = baseline;
    invalid.source_output_sha256 = std::string(64U, 'A');
    require(validate_export_request(invalid, source_output).error == ExportRequestError::InvalidSourceOutputDigest, "uppercase digest must be rejected");

    invalid = baseline;
    invalid.source_output_id = "stale-output";
    require(validate_export_request(invalid, source_output).error == ExportRequestError::SourceOutputIdentityMismatch, "source output id substitution must fail");

    invalid = baseline;
    invalid.source_output_sha256 = std::string(64U, 'b');
    require(validate_export_request(invalid, source_output).error == ExportRequestError::SourceOutputDigestMismatch, "source digest substitution must fail");

    invalid = baseline;
    invalid.schema_version = "vektoryum.export.v1\nexport_id=collision";
    require(validate_export_request(invalid, source_output).error == ExportRequestError::UnsafeTextField, "schema delimiter injection must fail");

    invalid = baseline;
    invalid.export_id = "export-0001\rsource_output_id=collision";
    require(validate_export_request(invalid, source_output).error == ExportRequestError::UnsafeTextField, "export id delimiter injection must fail");

    invalid = baseline;
    invalid.source_output_id = "hybrid-output-0001\nwidth=1";
    require(validate_export_request(invalid, source_output).error == ExportRequestError::UnsafeTextField, "source id delimiter injection must fail");

    invalid = baseline;
    invalid.width = 0U;
    require(validate_export_request(invalid, source_output).error == ExportRequestError::ZeroDimension, "zero width must fail");

    invalid = baseline;
    invalid.estimated_output_bytes = 0U;
    require(validate_export_request(invalid, source_output).error == ExportRequestError::ZeroEstimatedOutputBytes, "zero output estimate must fail");

    invalid = baseline;
    invalid.estimated_output_bytes = 2049U;
    ExportLimits tight_limits{};
    tight_limits.max_output_bytes = 2048U;
    require(validate_export_request(invalid, source_output, tight_limits).error == ExportRequestError::OutputBudgetExceeded, "output budget overflow must fail");

    invalid = baseline;
    invalid.width = 100U;
    invalid.height = 100U;
    tight_limits = ExportLimits{};
    tight_limits.max_pixels = 9999U;
    require(validate_export_request(invalid, source_output, tight_limits).error == ExportRequestError::PixelBudgetExceeded, "pixel budget overflow must fail");

    const std::string report_a = canonical_export_request_report(baseline);
    const std::string report_b = canonical_export_request_report(baseline);
    require(report_a == report_b, "canonical report must be deterministic");
    require(report_a.find("format=svg\n") != std::string::npos, "canonical report must contain format");
    require(report_a.find("source_output_id=hybrid-output-0001\n") != std::string::npos, "canonical report must contain source id");
    require(report_a.find("source_output_sha256=" + std::string(64U, 'a') + "\n") != std::string::npos, "canonical report must contain source digest");

    return 0;
}
