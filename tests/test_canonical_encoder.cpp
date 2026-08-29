#include "vektoryum/export/canonical_encoder.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

using vektoryum::exporting::ExportFormat;
using vektoryum::exporting::ExportRequest;
using vektoryum::hybrid::HybridOutputManifest;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
}

HybridOutputManifest source_output() {
    HybridOutputManifest output;
    output.output_id = "hybrid-output-1";
    output.output_sha256 = std::string(64U, 'a');
    return output;
}

ExportRequest request_for(ExportFormat format) {
    ExportRequest request;
    request.schema_version = "vektoryum.export.v1";
    request.export_id = "export-1";
    request.format = format;
    request.source_output_id = "hybrid-output-1";
    request.source_output_sha256 = std::string(64U, 'a');
    request.width = 640U;
    request.height = 480U;
    request.estimated_output_bytes = 4096U;
    return request;
}

}  // namespace

int main() {
    const HybridOutputManifest source = source_output();
    const std::array formats{ExportFormat::Svg, ExportFormat::Pdf, ExportFormat::Eps, ExportFormat::Dxf};

    for (const ExportFormat format : formats) {
        const ExportRequest request = request_for(format);
        const auto first = vektoryum::exporting::encode_canonical_export(request, source);
        const auto second = vektoryum::exporting::encode_canonical_export(request, source);
        if (!first.ok() || !second.ok()) {
            fail("canonical encoder rejected a valid request");
        }
        if (first.artifact.bytes != second.artifact.bytes ||
            first.artifact.output_sha256 != second.artifact.output_sha256) {
            fail("identical validated input did not produce identical bytes and digest");
        }
        const auto validation = vektoryum::exporting::validate_encoded_export_artifact(
            request, source, first.artifact);
        if (!validation.ok()) {
            fail("canonical encoder produced an artifact rejected by Stage 10 validation");
        }
    }

    ExportRequest invalid = request_for(ExportFormat::Svg);
    invalid.source_output_sha256 = std::string(64U, 'b');
    if (vektoryum::exporting::encode_canonical_export(invalid, source).ok()) {
        fail("canonical encoder accepted substituted source provenance");
    }

    ExportRequest too_small = request_for(ExportFormat::Svg);
    too_small.estimated_output_bytes = 1U;
    if (vektoryum::exporting::encode_canonical_export(too_small, source).ok()) {
        fail("canonical encoder exceeded the caller output-byte budget");
    }

    return EXIT_SUCCESS;
}
