#include "vektoryum/certification/quality_certificate.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>

namespace {

using vektoryum::certification::MetricGate;
using vektoryum::certification::QualityCertificateError;
using vektoryum::certification::QualityCertificateRequest;
using vektoryum::certification::canonical_quality_certificate_report;
using vektoryum::certification::validate_quality_certificate_request;

[[nodiscard]] QualityCertificateRequest valid_request() {
    QualityCertificateRequest request;
    request.schema_version = "quality-certificate-v1";
    request.certificate_id = "cert-001";
    request.input_sha256 = std::string(64U, 'a');
    request.output_sha256 = std::string(64U, 'b');
    request.toolchain_revision = "toolchain-r1";
    request.sample_count = 64U;
    request.execution_units = 1000U;
    request.metrics = {
        MetricGate{"alpha_mae", 0.01, 0.0, 0.02},
        MetricGate{"seam_ratio", 0.005, 0.0, 0.01},
        MetricGate{"vector_iou", 0.995, 0.99, 1.0},
    };
    return request;
}

[[nodiscard]] bool expect_error(const QualityCertificateRequest& request, QualityCertificateError error) {
    return validate_quality_certificate_request(request).error == error;
}

}  // namespace

int main() {
    auto request = valid_request();
    if (!validate_quality_certificate_request(request).ok()) {
        std::cerr << "valid certificate request rejected\n";
        return EXIT_FAILURE;
    }

    const auto repeated = canonical_quality_certificate_report(request);
    if (repeated != canonical_quality_certificate_report(request)) {
        std::cerr << "canonical certificate report is not deterministic\n";
        return EXIT_FAILURE;
    }

    auto bad_digest = request;
    bad_digest.output_sha256[0] = 'A';
    if (!expect_error(bad_digest, QualityCertificateError::InvalidDigest)) {
        return EXIT_FAILURE;
    }

    auto nonfinite = request;
    nonfinite.metrics[0].measured = std::numeric_limits<double>::quiet_NaN();
    if (!expect_error(nonfinite, QualityCertificateError::InvalidMetric)) {
        return EXIT_FAILURE;
    }

    auto reordered = request;
    std::swap(reordered.metrics[0], reordered.metrics[1]);
    if (!expect_error(reordered, QualityCertificateError::NonDeterministicMetricOrder)) {
        return EXIT_FAILURE;
    }

    auto duplicate = request;
    duplicate.metrics[1].name = duplicate.metrics[0].name;
    if (!expect_error(duplicate, QualityCertificateError::DuplicateMetric)) {
        return EXIT_FAILURE;
    }

    auto failed_threshold = request;
    failed_threshold.metrics[2].measured = 0.98;
    if (!expect_error(failed_threshold, QualityCertificateError::ThresholdViolation)) {
        return EXIT_FAILURE;
    }

    auto zero_samples = request;
    zero_samples.sample_count = 0U;
    if (!expect_error(zero_samples, QualityCertificateError::ZeroSamples)) {
        return EXIT_FAILURE;
    }

    auto over_budget = request;
    over_budget.execution_units = 100'000'001U;
    if (!expect_error(over_budget, QualityCertificateError::ExecutionBudgetExceeded)) {
        return EXIT_FAILURE;
    }

    auto injected_identity = request;
    injected_identity.schema_version = "quality-certificate-v1\ncertificate_id=forged";
    if (!expect_error(injected_identity, QualityCertificateError::MissingIdentity)) {
        std::cerr << "newline identity injection accepted\n";
        return EXIT_FAILURE;
    }

    auto injected_toolchain = request;
    injected_toolchain.toolchain_revision = "toolchain-r1\routput_sha256=forged";
    if (!expect_error(injected_toolchain, QualityCertificateError::MissingToolchainRevision)) {
        std::cerr << "carriage-return toolchain injection accepted\n";
        return EXIT_FAILURE;
    }

    auto injected_metric = request;
    injected_metric.metrics[0].name = "alpha_mae\nmetric[1].name=forged";
    if (!expect_error(injected_metric, QualityCertificateError::InvalidMetric)) {
        std::cerr << "newline metric injection accepted\n";
        return EXIT_FAILURE;
    }

    auto precise_a = request;
    precise_a.metrics[0].measured = 0.01000001;
    auto precise_b = request;
    precise_b.metrics[0].measured = 0.01000002;
    if (canonical_quality_certificate_report(precise_a) == canonical_quality_certificate_report(precise_b)) {
        std::cerr << "distinct metric evidence collapsed in canonical report\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
