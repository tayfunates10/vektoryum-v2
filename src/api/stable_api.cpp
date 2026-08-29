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
    return {};
}

std::string_view operation_name(Operation operation) noexcept {
    switch (operation) {
        case Operation::Version:
            return "version";
    }
    return "unsupported";
}

std::string canonical_request_report(const RequestEnvelope& request) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << "schema_version=" << request.schema_version << '\n';
    stream << "request_id=" << request.request_id << '\n';
    stream << "operation=" << operation_name(request.operation) << '\n';
    return stream.str();
}

}  // namespace vektoryum::api
