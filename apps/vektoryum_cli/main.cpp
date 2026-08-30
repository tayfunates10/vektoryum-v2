#include <iostream>
#include <string>
#include <string_view>

#include "vektoryum/api/stable_api.hpp"
#include "vektoryum/certification/quality_certificate.hpp"
#include "vektoryum/export/canonical_encoder.hpp"
#include "vektoryum/io/raster_input.hpp"
#include "vektoryum/version.hpp"

namespace {

int print_version() {
    std::cout << "Vektoryum v2 core " << vektoryum::version_string() << '\n';
    return static_cast<int>(vektoryum::api::ExitCode::Success);
}

int probe_raster_input(std::string_view path) {
    const auto loaded = vektoryum::io::load_raster_input(std::string(path));
    if (!loaded.ok()) {
        std::cout << "schema_version=vektoryum.raster-input.v1\n"
                  << "status=error\n"
                  << "error=" << vektoryum::io::raster_input_error_name(loaded.error) << '\n';
        return static_cast<int>(vektoryum::api::ExitCode::Data);
    }

    std::cout << "schema_version=vektoryum.raster-input.v1\n"
              << "status=accepted\n"
              << "format=" << vektoryum::io::raster_format_name(loaded.input.format) << '\n'
              << "input_bytes=" << loaded.input.bytes.size() << '\n';
    return static_cast<int>(vektoryum::api::ExitCode::Success);
}

vektoryum::certification::QualityCertificateIssueResult issue_cli_stage11_certificate() {
    vektoryum::hybrid::HybridOutputManifest source{};
    source.output_id = "hybrid-output-stage12-cli-0001";
    source.output_sha256 = std::string(64U, 'a');
    source.seam_error = 0.005;

    const vektoryum::exporting::ExportRequest export_request{
        "vektoryum.export.v1",
        "export-stage12-cli-0001",
        vektoryum::exporting::ExportFormat::Svg,
        source.output_id,
        source.output_sha256,
        32U,
        32U,
        4096U,
    };
    const auto encoded = vektoryum::exporting::encode_canonical_export(export_request, source);
    if (!encoded.ok()) {
        return {};
    }

    vektoryum::certification::QualityCertificateRequest certificate_request;
    certificate_request.schema_version = "quality-certificate-v1";
    certificate_request.certificate_id = "stage11-cli-certificate-0001";
    certificate_request.input_sha256 = encoded.artifact.source_output_sha256;
    certificate_request.output_sha256 = encoded.artifact.output_sha256;
    certificate_request.toolchain_revision = "stage11-cli-toolchain-0001";
    certificate_request.sample_count = 64U;
    certificate_request.execution_units = 1000U;
    certificate_request.metrics = {
        vektoryum::certification::MetricGate{"alpha_mae", 0.01, 0.0, 0.02},
        vektoryum::certification::MetricGate{"seam_ratio", 0.005, 0.0, 0.01},
        vektoryum::certification::MetricGate{"vector_iou", 0.995, 0.99, 1.0},
    };

    return vektoryum::certification::issue_quality_certificate(
        certificate_request,
        export_request,
        source,
        encoded.artifact);
}

int run_certified_export(std::string_view request_id, std::string_view supplied_digest) {
    const auto issued = issue_cli_stage11_certificate();
    if (!issued.ok()) {
        std::cerr << "error: stage11 certificate issuance failed\n";
        return static_cast<int>(vektoryum::api::ExitCode::Software);
    }

    const std::string certificate_digest = supplied_digest.empty()
        ? issued.artifact.certificate_sha256
        : std::string(supplied_digest);
    const vektoryum::api::CertifiedOperationRequest request{
        vektoryum::api::RequestEnvelope{
            std::string(vektoryum::api::stable_schema_version),
            std::string(request_id),
            vektoryum::api::Operation::CertifiedExport,
            4096U,
            8192U,
            200U,
        },
        certificate_digest,
    };
    const auto response = vektoryum::api::execute_certified_operation(request, issued.artifact);
    std::cout << vektoryum::api::canonical_certified_response_report(response);
    return static_cast<int>(response.response.exit_code);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 1) {
        return print_version();
    }

    if (argc == 2) {
        const std::string_view argument{argv[1]};
        if (argument == "--version") {
            return print_version();
        }
        if (argument == "--help") {
            std::cout << "usage: vektoryum_cli [--version|--help|--probe-input FILE|--certified-export REQUEST_ID [CERTIFICATE_SHA256]]\n";
            return static_cast<int>(vektoryum::api::ExitCode::Success);
        }
    }

    if (argc == 3 && std::string_view{argv[1]} == "--probe-input") {
        return probe_raster_input(argv[2]);
    }

    if ((argc == 3 || argc == 4) && std::string_view{argv[1]} == "--certified-export") {
        const std::string_view supplied_digest = argc == 4 ? std::string_view{argv[3]} : std::string_view{};
        return run_certified_export(argv[2], supplied_digest);
    }

    std::cerr << "error: unsupported command line\n";
    return static_cast<int>(vektoryum::api::ExitCode::Usage);
}
