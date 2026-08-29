#include "vektoryum/api/stable_api.hpp"

#include <cassert>
#include <string>

using vektoryum::api::ExitCode;
using vektoryum::api::Operation;
using vektoryum::api::RequestEnvelope;
using vektoryum::api::RequestError;
using vektoryum::api::RequestLimits;
using vektoryum::api::ResponseEnvelope;
using vektoryum::api::ResponseStatus;
using vektoryum::api::canonical_request_report;
using vektoryum::api::canonical_response_report;
using vektoryum::api::stable_api_major;
using vektoryum::api::stable_api_minor;
using vektoryum::api::stable_schema_version;
using vektoryum::api::validate_request_envelope;

namespace {

[[nodiscard]] RequestEnvelope valid_request() {
    return RequestEnvelope{
        std::string(stable_schema_version),
        "request-0001",
        Operation::Version,
    };
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

    const RequestEnvelope baseline = valid_request();
    assert(validate_request_envelope(baseline).ok());

    const std::string report_a = canonical_request_report(baseline);
    const std::string report_b = canonical_request_report(baseline);
    assert(report_a == report_b);
    assert(report_a ==
           "schema_version=vektoryum.core-api.v1\n"
           "request_id=request-0001\n"
           "operation=version\n");

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

    return 0;
}
