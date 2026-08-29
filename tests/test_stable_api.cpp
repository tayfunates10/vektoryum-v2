#include "vektoryum/api/stable_api.hpp"

#include <cassert>
#include <string>
#include <vector>

#include "vektoryum/certification/quality_certificate.hpp"
#include "vektoryum/ml/artifact_digest.hpp"

using vektoryum::api::CertifiedOperationRequest;
using vektoryum::api::ExitCode;
using vektoryum::api::Operation;
using vektoryum::api::RequestEnvelope;
using vektoryum::api::RequestError;
using vektoryum::api::RequestLimits;
using vektoryum::api::ResponseEnvelope;
using vektoryum::api::ResponseStatus;
using vektoryum::api::canonical_certified_request_report;
using vektoryum::api::canonical_request_report;
using vektoryum::api::canonical_response_report;
using vektoryum::api::stable_api_major;
using vektoryum::api::stable_api_minor;
using vektoryum::api::stable_schema_version;
using vektoryum::api::validate_certified_operation_request;
using vektoryum::api::validate_request_envelope;
using vektoryum::certification::QualityCertificateArtifact;

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

[[nodiscard]] QualityCertificateArtifact valid_certificate() {
    QualityCertificateArtifact artifact;
    artifact.certificate_id = "stage11-certificate-0001";
    artifact.input_sha256 = std::string(64U, '1');
    artifact.output_sha256 = std::string(64U, '2');
    artifact.toolchain_revision = "stage11-toolchain-0001";
    const std::string report =
        "schema_version=vektoryum.quality-certificate.v1\n"
        "certificate_id=stage11-certificate-0001\n"
        "input_sha256=" + artifact.input_sha256 + "\n"
        "output_sha256=" + artifact.output_sha256 + "\n"
        "toolchain_revision=stage11-toolchain-0001\n";
    artifact.canonical_bytes.assign(report.begin(), report.end());
    artifact.certificate_sha256 = vektoryum::ml::sha256_hex(artifact.canonical_bytes);
    return artifact;
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

    const QualityCertificateArtifact certificate = valid_certificate();
    CertifiedOperationRequest certified{
        RequestEnvelope{
            std::string(stable_schema_version),
            "request-certified-export-0001",
            Operation::CertifiedExport,
            4096U,
            8192U,
            200U,
        },
        certificate.certificate_sha256,
    };

    assert(validate_request_envelope(certified.request).error == RequestError::MissingCertificateEvidence);
    assert(validate_certified_operation_request(certified, certificate).ok());
    const std::string certified_a = canonical_certified_request_report(certified);
    const std::string certified_b = canonical_certified_request_report(certified);
    assert(certified_a == certified_b);
    assert(certified_a.find("operation=certified_export\n") != std::string::npos);
    assert(certified_a.find("certificate_sha256=" + certificate.certificate_sha256 + "\n") != std::string::npos);

    CertifiedOperationRequest missing_certificate = certified;
    missing_certificate.certificate_sha256.clear();
    assert(validate_certified_operation_request(missing_certificate, certificate).error ==
           RequestError::MissingCertificateEvidence);

    CertifiedOperationRequest substituted = certified;
    substituted.certificate_sha256 = std::string(64U, 'a');
    assert(validate_certified_operation_request(substituted, certificate).error ==
           RequestError::CertificateProvenanceMismatch);

    QualityCertificateArtifact tampered = certificate;
    tampered.canonical_bytes.push_back(static_cast<std::uint8_t>('x'));
    assert(validate_certified_operation_request(certified, tampered).error == RequestError::InvalidCertificateArtifact);

    QualityCertificateArtifact malformed = certificate;
    malformed.certificate_sha256 = "not-a-digest";
    assert(validate_certified_operation_request(certified, malformed).error == RequestError::InvalidCertificateArtifact);

    CertifiedOperationRequest wrong_operation = certified;
    wrong_operation.request.operation = Operation::Version;
    assert(validate_certified_operation_request(wrong_operation, certificate).error == RequestError::UnsupportedOperation);

    return 0;
}
