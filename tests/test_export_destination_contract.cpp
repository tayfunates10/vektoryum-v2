#include "vektoryum/export/canonical_encoder.hpp"
#include "vektoryum/export/export_destination_contract.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

using vektoryum::exporting::ExportDestination;
using vektoryum::exporting::ExportDestinationError;
using vektoryum::exporting::ExportFormat;
using vektoryum::exporting::ExportMetadataEntry;
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
        const auto encoded = vektoryum::exporting::encode_canonical_export(request, source);
        if (!encoded.ok()) {
            fail("canonical exporter rejected valid end-to-end fixture");
        }

        ExportDestination destination;
        destination.relative_path = "exports/final-output.bin";
        destination.metadata = {ExportMetadataEntry{"title", "deterministic export"},
                                ExportMetadataEntry{"producer", "vektoryum-v2"}};
        const auto validation = vektoryum::exporting::validate_export_destination(
            request, encoded.artifact, destination);
        if (!validation.ok()) {
            fail("safe destination rejected valid canonical export artifact");
        }
        const std::string first_report = vektoryum::exporting::canonical_export_destination_report(destination);
        const std::string second_report = vektoryum::exporting::canonical_export_destination_report(destination);
        if (first_report != second_report || first_report.find("metadata_count=2\n") == std::string::npos) {
            fail("destination report is not deterministic");
        }

        ExportDestination hostile = destination;
        hostile.relative_path = "../escape.bin";
        if (vektoryum::exporting::validate_export_destination(request, encoded.artifact, hostile).error !=
            ExportDestinationError::UnsafePath) {
            fail("parent traversal path was accepted");
        }
        hostile = destination;
        hostile.relative_path = "/absolute/output.bin";
        if (vektoryum::exporting::validate_export_destination(request, encoded.artifact, hostile).error !=
            ExportDestinationError::UnsafePath) {
            fail("absolute path was accepted");
        }
        hostile = destination;
        hostile.relative_path = "C:/windows/output.bin";
        if (vektoryum::exporting::validate_export_destination(request, encoded.artifact, hostile).error !=
            ExportDestinationError::UnsafePath) {
            fail("drive-qualified path was accepted");
        }
        hostile = destination;
        hostile.metadata = {ExportMetadataEntry{"source_output_sha256", std::string(64U, 'b')}};
        if (vektoryum::exporting::validate_export_destination(request, encoded.artifact, hostile).error !=
            ExportDestinationError::ReservedMetadataKey) {
            fail("metadata was allowed to substitute provenance");
        }
        hostile = destination;
        hostile.metadata = {ExportMetadataEntry{"title", "safe\nsource_output_id=fake"}};
        if (vektoryum::exporting::validate_export_destination(request, encoded.artifact, hostile).error !=
            ExportDestinationError::UnsafeMetadataText) {
            fail("metadata serialization injection was accepted");
        }
        hostile = destination;
        hostile.metadata = {ExportMetadataEntry{"title", "one"}, ExportMetadataEntry{"title", "two"}};
        if (vektoryum::exporting::validate_export_destination(request, encoded.artifact, hostile).error !=
            ExportDestinationError::DuplicateMetadataKey) {
            fail("duplicate metadata key was accepted");
        }

        auto substituted = encoded.artifact;
        substituted.source_output_sha256 = std::string(64U, 'b');
        if (vektoryum::exporting::validate_export_destination(request, substituted, destination).error !=
            ExportDestinationError::ArtifactDigestMismatch) {
            fail("destination accepted substituted artifact provenance");
        }
    }

    return EXIT_SUCCESS;
}
