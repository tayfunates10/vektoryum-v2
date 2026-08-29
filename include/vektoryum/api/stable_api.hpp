#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

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
};

struct RequestLimits {
    std::size_t max_request_id_bytes{128U};
};

struct RequestEnvelope {
    std::string schema_version;
    std::string request_id;
    Operation operation{Operation::Unspecified};
};

enum class RequestError : std::uint8_t {
    None = 0,
    UnsupportedSchema,
    EmptyRequestId,
    RequestIdTooLarge,
    UnsafeRequestId,
    UnsupportedOperation,
};

struct RequestValidation {
    RequestError error{RequestError::None};

    [[nodiscard]] bool ok() const noexcept { return error == RequestError::None; }
};

[[nodiscard]] RequestValidation validate_request_envelope(
    const RequestEnvelope& request,
    const RequestLimits& limits = {}) noexcept;

[[nodiscard]] std::string canonical_request_report(const RequestEnvelope& request);
[[nodiscard]] std::string_view operation_name(Operation operation) noexcept;

}  // namespace vektoryum::api
