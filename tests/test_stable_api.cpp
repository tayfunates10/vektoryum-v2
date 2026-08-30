#include "vektoryum/api/stable_api.hpp"

#include <cassert>
#include <string>
#include <vector>

#include "vektoryum/certification/quality_certificate.hpp"
#include "vektoryum/export/canonical_encoder.hpp"

using vektoryum::api::CertifiedOperationRequest;
using vektoryum::api::CertifiedOperationResponse;
using vektoryum::api::ExitCode;
using vektoryum::api::Operation;
using vektoryum::api::RequestEnvelope;
using vektoryum::api::RequestError;
using vektoryum::api::RequestLimits;
using vektoryum::api::ResponseEnvelope;
using vektoryum::api::ResponseStatus;
using vektoryum::api::canonical_certified_request_report;
using vektoryum::api::canonical_certified_response_report;
using vektoryum::api::canonical_request_report;
using vektoryum::api::canonical_response_report;
using vektoryum::api::execute_certified_operation;
using vektoryum::api::stable_api_major;
using vektoryum::api::stable_api_minor;
using vektoryum::api::stable_schema_version;
using vektoryum::api::validate_certified_operation_request;
using vektoryum::api::validate_request_envelope;
using vektoryum::certification::MetricGate;
using vektoryum::certification::QualityCertificateArtifact;
using vektoryum::certification::QualityCertificateRequest;
using vektoryum::certification::issue_quality_certificate;
using vektoryum::exporting::ExportFormat;
using vektoryum::exporting::ExportRequest;
using vektoryum::exporting::encode_canonical_export;
using vektoryum::hybrid::HybridOutputManifest;

namespace {

[[nodiscard]] RequestEnvelope valid_request() {
    return RequestEnvelope{
        std::string(stable_schema_version),
        "request-0001",
        Operation::Version,
        1024U,
        2048U,
        100U,
    };
}

[[nodiscard]] QualityCertificateArtifact issued_stage11_certificate() {
    HybridOutputManifest source{};
    source.output_id = "hybrid-output-stage12-0001";
    source.output_sha256 = std::string(64U, 'a');
    source.seam_error = 0.005;

    const ExportRequest export_request{
        "vektoryum.export.v1",
        "export-stage12-0001",
        ExportFormat::Svg,
        source.output_id,
        source.output_sha256,
        32U,
        32U,
        4096U,
    };
    const auto encoded = encode_canonical_export(export_request, source);
    assert(encoded.ok());

    QualityCertificateRequest certificate_request;
    certificate_request.schema_version = "quality-certificate-v1";
    certificate_request.certificate_id = "stage11-issued-certificate-0001";
    certificate_request.input_sha256 = encoded.artifact.source_output_sha256;
    certificate_request.output_sha256 = encoded.artifact.output_sha256;
    certificate_request.toolchain_revision = "stage11-toolchain-0001";
    certificate_request.sample_count = 64U;
    certificate_request.execution_units = 1000U;
    certificate_request.metrics = {
        MetricGate{"alpha_mae", 0.01, 0.0, 0.02},
        MetricGate{"seam_ratio", 0.005, 0.0, 0.01},
        MetricGate{"vector_iou", 0.995, 0.99, 1.0},
    };

    const auto issued = issue_quality_certificate(certificate_request, export_request, source, encoded.artifact);
    assert(issued.ok());
    return issued.artifact;
}

}  // namespace

int main() {
    static_assert(stable_api_major == 1U);
    static_assert(stable_api_minor == 0U);
    static_assert(static_cast<int>(ExitCode::Success) == 0);
    static_assert(static_cast<int>(ExitCode::Usage) == 64);
    static_assert(static_cast<int>(ExitCode::Data) == 65);
    static_assert(static_cast<int>(ExitCode::Software) == 70);
    static_assert(static_cast<int>(Operation::Unspecified) == 0);
    static_assert(static_cast<int>(Operation::Version) == 1);
    static_assert(static_cast<int>(Operation::CertifiedExport) == 2);

    const RequestEnvelope baseline = valid_request();
    assert(validate_request_envelope(baseline).ok());

    const std::string report_a = canonical_request_report(baseline);
    const std::string report_b = canonical_request_report(baseline);
    assert(report_a == report_b);
    assert(report_a ==
           "schema_version=vektoryum.core-api.v1\n"
           "request_id=request-0001\n"
           "operation=version\n"
           "input_bytes=1024\n"
           "output_bytes=2048\n"
           "work_units=100\n");

    const ResponseEnvelope success{
        std::string(stable_schema_version),
        baseline.request_id,
        Operation::Version,
        ResponseStatus::Success,
        ExitCode::Success,
        RequestError::None,
    };
    const std::string success_a = canonical_response_report(success);
    const std::string success_b = canonical_response_report(success);
    assert(success_a == success_b);
    assert(success_a ==
           "schema_version=vektoryum.core-api.v1\n"
           "request_id=request-0001\n"
           "operation=version\n"
           "status=success\n"
           "exit_code=0\n"
           "error=none\n");

    const ResponseEnvelope failure{
        std::string(stable_schema_version),
        baseline.request_id,
        Operation::Version,
        ResponseStatus::Error,
        ExitCode::Data,
        RequestError::UnsupportedSchema,
    };
    assert(canonical_response_report(failure) ==
           "schema_version=vektoryum.core-api.v1\n"
           "request_id=request-0001\n"
           "operation=version\n"
           "status=error\n"
           "exit_code=65\n"
           "error=unsupported_schema\n");

    RequestEnvelope invalid = baseline;
    invalid.schema_version = "vektoryum.core-api.v2";
    assert(validate_request_envelope(invalid).error == RequestError::UnsupportedSchema);

    invalid = baseline;
    invalid.request_id.clear();
    assert(validate_request_envelope(invalid).error == RequestError::EmptyRequestId);

    invalid = baseline;
    invalid.request_id = "request-0001\noperation=collision";
    assert(validate_request_envelope(invalid).error == RequestError::UnsafeRequestId);

    invalid = baseline;
    invalid.request_id = std::string(129U, 'x');
    RequestLimits limits{};
    limits.max_request_id_bytes = 128U;
    assert(validate_request_envelope(invalid, limits).error == RequestError::RequestIdTooLarge);

    invalid = baseline;
    invalid.operation = static_cast<Operation>(255);
    assert(validate_request_envelope(invalid).error == RequestError::UnsupportedOperation);

    RequestEnvelope omitted_operation{};
    omitted_operation.schema_version = std::string(stable_schema_version);
    omitted_operation.request_id = "request-omitted-operation";
    assert(omitted_operation.operation == Operation::Unspecified);
    assert(validate_request_envelope(omitted_operation).error == RequestError::UnsupportedOperation);

    limits = {};
    invalid = baseline;
    invalid.input_bytes = limits.max_input_bytes;
    invalid.output_bytes = limits.max_output_bytes;
    invalid.work_units = limits.max_work_units;
    assert(validate_request_envelope(invalid, limits).ok());

    invalid.input_bytes = limits.max_input_bytes + 1U;
    assert(validate_request_envelope(invalid, limits).error == RequestError::InputTooLarge);

    invalid = baseline;
    invalid.output_bytes = limits.max_output_bytes + 1U;
    assert(validate_request_envelope(invalid, limits).error == RequestError::OutputTooLarge);

    invalid = baseline;
    invalid.work_units = limits.max_work_units + 1U;
    assert(validate_request_envelope(invalid, limits).error == RequestError::WorkLimitExceeded);

    const QualityCertificateArtifact certificate_a = issued_stage11_certificate();
    const QualityCertificateArtifact certificate_b = issued_stage11_certificate();
    assert(certificate_a.canonical_bytes == certificate_b.canonical_bytes);
    assert(certificate_a.certificate_sha256 == certificate_b.certificate_sha256);

    CertifiedOperationRequest certified{
        RequestEnvelope{
            std::string(stable_schema_version),
            "request-certified-export-0001",
            Operation::CertifiedExport,
            4096U,
            8192U,
            200U,
        },
        certificate_a.certificate_sha256,
    };

    assert(validate_request_envelope(certified.request).error == RequestError::MissingCertificateEvidence);
    assert(validate_certified_operation_request(certified, certificate_a).ok());
    assert(validate_certified_operation_request(certified, certificate_b).ok());
    const std::string certified_a = canonical_certified_request_report(certified);
    const std::string certified_b = canonical_certified_request_report(certified);
    assert(certified_a == certified_b);
    assert(certified_a.find("operation=certified_export\n") != std::string::npos);
    assert(certified_a.find("certificate_sha256=" + certificate_a.certificate_sha256 + "\n") != std::string::npos);

    const CertifiedOperationResponse executed_a = execute_certified_operation(certified, certificate_a);
    const CertifiedOperationResponse executed_b = execute_certified_operation(certified, certificate_b);
    const std::string executed_report_a = canonical_certified_response_report(executed_a);
    const std::string executed_report_b = canonical_certified_response_report(executed_b);
    assert(executed_report_a == executed_report_b);
    assert(executed_a.response.status == ResponseStatus::Success);
    assert(executed_a.response.exit_code == ExitCode::Success);
    assert(executed_a.response.error == RequestError::None);
    assert(executed_a.certificate_sha256 == certificate_a.certificate_sha256);
    assert(executed_report_a.find("certificate_sha256=" + certificate_a.certificate_sha256 + "\n") != std::string::npos);

    CertifiedOperationRequest missing_certificate = certified;
    missing_certificate.certificate_sha256.clear();
    assert(validate_certified_operation_request(missing_certificate, certificate_a).error ==
           RequestError::MissingCertificateEvidence);

    CertifiedOperationRequest substituted = certified;
    substituted.certificate_sha256 = std::string(64U, 'b');
    assert(validate_certified_operation_request(substituted, certificate_a).error ==
           RequestError::CertificateProvenanceMismatch);
    const CertifiedOperationResponse substituted_response = execute_certified_operation(substituted, certificate_a);
    assert(substituted_response.response.status == ResponseStatus::Error);
    assert(substituted_response.response.exit_code == ExitCode::Data);
    assert(substituted_response.response.error == RequestError::CertificateProvenanceMismatch);
    assert(substituted_response.certificate_sha256.empty());
    assert(canonical_certified_response_report(substituted_response).find("certificate_sha256=\n") != std::string::npos);

    QualityCertificateArtifact substituted_artifact = certificate_a;
    substituted_artifact.certificate_sha256 = std::string(64U, 'b');
    assert(validate_certified_operation_request(certified, substituted_artifact).error ==
           RequestError::InvalidCertificateArtifact);

    QualityCertificateArtifact tampered = certificate_a;
    tampered.canonical_bytes.push_back(static_cast<std::uint8_t>('x'));
    assert(validate_certified_operation_request(certified, tampered).error == RequestError::InvalidCertificateArtifact);

    QualityCertificateArtifact malformed = certificate_a;
    malformed.certificate_sha256 = "not-a-digest";
    assert(validate_certified_operation_request(certified, malformed).error == RequestError::InvalidCertificateArtifact);

    CertifiedOperationRequest wrong_operation = certified;
    wrong_operation.request.operation = Operation::Version;
    assert(validate_certified_operation_request(wrong_operation, certificate_a).error == RequestError::UnsupportedOperation);

    return 0;
}
