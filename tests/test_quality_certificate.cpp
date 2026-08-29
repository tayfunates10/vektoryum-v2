#include "vektoryum/certification/quality_certificate.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>

namespace {

using vektoryum::certification::MetricGate;
using vektoryum::certification::QualityCertificateError;
using vektoryum::certification::QualityCertificateRequest;
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

    const auto repeated = vektoryum::certification::canonical_quality_certificate_report(request);
    if (repeated != vektoryum::certification::canonical_quality_certificate_report(request)) {
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

    return EXIT_SUCCESS;
}
