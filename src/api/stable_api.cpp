#include "vektoryum/api/stable_api.hpp"

#include <locale>
#include <sstream>

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

[[nodiscard]] bool supported_operation(Operation operation) noexcept {
    return operation == Operation::Version;
}

}  // namespace

RequestValidation validate_request_envelope(
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
    if (!supported_operation(request.operation)) {
        return {RequestError::UnsupportedOperation};
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

std::string_view operation_name(Operation operation) noexcept {
    switch (operation) {
        case Operation::Unspecified:
            return "unsupported";
        case Operation::Version:
            return "version";
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
