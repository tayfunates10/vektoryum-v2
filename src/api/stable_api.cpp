#include "vektoryum/api/stable_api.hpp"

#include <algorithm>
#include <locale>
#include <sstream>

#include "vektoryum/certification/quality_certificate.hpp"
#include "vektoryum/ml/artifact_digest.hpp"

namespace vektoryum::api {
namespace {

[[nodiscard]] bool has_report_delimiter(std::string_view value) noexcept {
    for (const char ch : value) {
        if (ch == '\n' || ch == '\r' || ch == '\0') {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool is_lower_hex_sha256(std::string_view value) noexcept {
    return value.size() == 64U && std::all_of(value.begin(), value.end(), [](char ch) {
               return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
           });
}

[[nodiscard]] RequestValidation validate_common_request(
    const RequestEnvelope& request,
    const RequestLimits& limits) noexcept {
    if (request.schema_version != stable_schema_version) {
        return {RequestError::UnsupportedSchema};
    }
    if (request.request_id.empty()) {
        return {RequestError::EmptyRequestId};
    }
    if (request.request_id.size() > limits.max_request_id_bytes) {
        return {RequestError::RequestIdTooLarge};
    }
    if (has_report_delimiter(request.request_id)) {
        return {RequestError::UnsafeRequestId};
    }
    if (request.input_bytes > limits.max_input_bytes) {
        return {RequestError::InputTooLarge};
    }
    if (request.output_bytes > limits.max_output_bytes) {
        return {RequestError::OutputTooLarge};
    }
    if (request.work_units > limits.max_work_units) {
        return {RequestError::WorkLimitExceeded};
    }
    return {};
}

}  // namespace

RequestValidation validate_request_envelope(
    const RequestEnvelope& request,
    const RequestLimits& limits) noexcept {
    const RequestValidation common = validate_common_request(request, limits);
    if (!common.ok()) {
        return common;
    }
    switch (request.operation) {
        case Operation::Version:
            return {};
        case Operation::CertifiedExport:
            return {RequestError::MissingCertificateEvidence};
        case Operation::Unspecified:
            return {RequestError::UnsupportedOperation};
    }
    return {RequestError::UnsupportedOperation};
}

RequestValidation validate_certified_operation_request(
    const CertifiedOperationRequest& request,
    const certification::QualityCertificateArtifact& certificate,
    const RequestLimits& limits) {
    const RequestValidation common = validate_common_request(request.request, limits);
    if (!common.ok()) {
        return common;
    }
    if (request.request.operation != Operation::CertifiedExport) {
        return {RequestError::UnsupportedOperation};
    }
    if (request.certificate_sha256.empty()) {
        return {RequestError::MissingCertificateEvidence};
    }
    if (!is_lower_hex_sha256(request.certificate_sha256) || !is_lower_hex_sha256(certificate.certificate_sha256) ||
        !is_lower_hex_sha256(certificate.input_sha256) || !is_lower_hex_sha256(certificate.output_sha256) ||
        certificate.certificate_id.empty() || certificate.toolchain_revision.empty() || certificate.canonical_bytes.empty() ||
        has_report_delimiter(certificate.certificate_id) || has_report_delimiter(certificate.toolchain_revision)) {
        return {RequestError::InvalidCertificateArtifact};
    }
    if (ml::sha256_hex(certificate.canonical_bytes) != certificate.certificate_sha256) {
        return {RequestError::InvalidCertificateArtifact};
    }
    if (request.certificate_sha256 != certificate.certificate_sha256) {
        return {RequestError::CertificateProvenanceMismatch};
    }
    return {};
}

std::string_view operation_name(Operation operation) noexcept {
    switch (operation) {
        case Operation::Unspecified:
            return "unsupported";
        case Operation::Version:
            return "version";
        case Operation::CertifiedExport:
            return "certified_export";
    }
    return "unsupported";
}

std::string_view request_error_name(RequestError error) noexcept {
    switch (error) {
        case RequestError::None:
            return "none";
        case RequestError::UnsupportedSchema:
            return "unsupported_schema";
        case RequestError::EmptyRequestId:
            return "empty_request_id";
        case RequestError::RequestIdTooLarge:
            return "request_id_too_large";
        case RequestError::UnsafeRequestId:
            return "unsafe_request_id";
        case RequestError::UnsupportedOperation:
            return "unsupported_operation";
        case RequestError::InputTooLarge:
            return "input_too_large";
        case RequestError::OutputTooLarge:
            return "output_too_large";
        case RequestError::WorkLimitExceeded:
            return "work_limit_exceeded";
        case RequestError::MissingCertificateEvidence:
            return "missing_certificate_evidence";
        case RequestError::InvalidCertificateArtifact:
            return "invalid_certificate_artifact";
        case RequestError::CertificateProvenanceMismatch:
            return "certificate_provenance_mismatch";
    }
    return "unsupported_operation";
}

std::string_view response_status_name(ResponseStatus status) noexcept {
    switch (status) {
        case ResponseStatus::Success:
            return "success";
        case ResponseStatus::Error:
            return "error";
    }
    return "error";
}

std::string canonical_request_report(const RequestEnvelope& request) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << "schema_version=" << request.schema_version << '\n';
    stream << "request_id=" << request.request_id << '\n';
    stream << "operation=" << operation_name(request.operation) << '\n';
    stream << "input_bytes=" << request.input_bytes << '\n';
    stream << "output_bytes=" << request.output_bytes << '\n';
    stream << "work_units=" << request.work_units << '\n';
    return stream.str();
}

std::string canonical_certified_request_report(const CertifiedOperationRequest& request) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << canonical_request_report(request.request);
    stream << "certificate_sha256=" << request.certificate_sha256 << '\n';
    return stream.str();
}

std::string canonical_response_report(const ResponseEnvelope& response) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << "schema_version=" << response.schema_version << '\n';
    stream << "request_id=" << response.request_id << '\n';
    stream << "operation=" << operation_name(response.operation) << '\n';
    stream << "status=" << response_status_name(response.status) << '\n';
    stream << "exit_code=" << static_cast<int>(response.exit_code) << '\n';
    stream << "error=" << request_error_name(response.error) << '\n';
    return stream.str();
}

}  // namespace vektoryum::api
