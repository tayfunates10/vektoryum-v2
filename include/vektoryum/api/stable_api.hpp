#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace vektoryum::certification {
struct QualityCertificateArtifact;
}

namespace vektoryum::api {

inline constexpr std::string_view stable_schema_version = "vektoryum.core-api.v1";
inline constexpr std::uint32_t stable_api_major = 1U;
inline constexpr std::uint32_t stable_api_minor = 0U;

enum class ExitCode : int {
    Success = 0,
    Usage = 64,
    Data = 65,
    Software = 70,
};

enum class Operation : std::uint8_t {
    Unspecified = 0,
    Version = 1,
    CertifiedExport = 2,
};

struct RequestLimits {
    std::size_t max_request_id_bytes{128U};
    std::uint64_t max_input_bytes{64U * 1024U * 1024U};
    std::uint64_t max_output_bytes{256U * 1024U * 1024U};
    std::uint64_t max_work_units{1'000'000U};
};

struct RequestEnvelope {
    std::string schema_version;
    std::string request_id;
    Operation operation{Operation::Unspecified};
    std::uint64_t input_bytes{0U};
    std::uint64_t output_bytes{0U};
    std::uint64_t work_units{0U};
};

struct CertifiedOperationRequest {
    RequestEnvelope request;
    std::string certificate_sha256;
};

enum class RequestError : std::uint8_t {
    None = 0,
    UnsupportedSchema,
    EmptyRequestId,
    RequestIdTooLarge,
    UnsafeRequestId,
    UnsupportedOperation,
    InputTooLarge,
    OutputTooLarge,
    WorkLimitExceeded,
    MissingCertificateEvidence,
    InvalidCertificateArtifact,
    CertificateProvenanceMismatch,
};

struct RequestValidation {
    RequestError error{RequestError::None};

    [[nodiscard]] bool ok() const noexcept { return error == RequestError::None; }
};

enum class ResponseStatus : std::uint8_t {
    Success = 0,
    Error = 1,
};

struct ResponseEnvelope {
    std::string schema_version;
    std::string request_id;
    Operation operation{Operation::Unspecified};
    ResponseStatus status{ResponseStatus::Error};
    ExitCode exit_code{ExitCode::Software};
    RequestError error{RequestError::UnsupportedOperation};
};

struct CertifiedOperationResponse {
    ResponseEnvelope response;
    std::string certificate_sha256;
};

[[nodiscard]] RequestValidation validate_request_envelope(
    const RequestEnvelope& request,
    const RequestLimits& limits = {}) noexcept;
[[nodiscard]] RequestValidation validate_certified_operation_request(
    const CertifiedOperationRequest& request,
    const certification::QualityCertificateArtifact& certificate,
    const RequestLimits& limits = {});
[[nodiscard]] CertifiedOperationResponse execute_certified_operation(
    const CertifiedOperationRequest& request,
    const certification::QualityCertificateArtifact& certificate,
    const RequestLimits& limits = {});

[[nodiscard]] std::string canonical_request_report(const RequestEnvelope& request);
[[nodiscard]] std::string canonical_certified_request_report(const CertifiedOperationRequest& request);
[[nodiscard]] std::string canonical_response_report(const ResponseEnvelope& response);
[[nodiscard]] std::string canonical_certified_response_report(const CertifiedOperationResponse& response);
[[nodiscard]] std::string_view operation_name(Operation operation) noexcept;
[[nodiscard]] std::string_view request_error_name(RequestError error) noexcept;
[[nodiscard]] std::string_view response_status_name(ResponseStatus status) noexcept;

}  // namespace vektoryum::api
