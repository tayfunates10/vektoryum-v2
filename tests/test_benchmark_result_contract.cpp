#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>

#include "vektoryum/ml/artifact_digest.hpp"
#include "vektoryum/training/benchmark_result_contract.hpp"

namespace {

int failures = 0;

void expect_true(bool condition, std::string_view name) {
    if (!condition) {
        ++failures;
        std::cerr << "[FAIL] " << name << '\n';
        return;
    }
    std::cout << "[PASS] " << name << '\n';
}

[[nodiscard]] std::vector<std::uint8_t> artifact_bytes() {
    return {0x56U, 0x45U, 0x4BU, 0x54U, 0x4FU, 0x52U, 0x59U, 0x55U, 0x4DU};
}

[[nodiscard]] vektoryum::training::BenchmarkResultManifest valid_manifest(
    const std::vector<std::uint8_t>& artifact) {
    using namespace vektoryum::training;
    BenchmarkResultManifest manifest;
    manifest.schema_version = "vektoryum-benchmark-result-v1";
    manifest.benchmark_id = "stage8-fidelity";
    manifest.benchmark_version = "1";
    manifest.run_id = "run-001";
    manifest.dataset_id = "dataset-a";
    manifest.dataset_version = "2026-08";
    manifest.model_id = "vektoryum-reference";
    manifest.model_version = "1";
    manifest.runtime_id = "vektoryum-reference-runtime";
    manifest.runtime_version = "1";
    manifest.seed = 0x5A17U;
    manifest.evaluated_samples = 128U;
    manifest.metrics = {
        BenchmarkMetric{"alpha_iou", "1", "ratio", "mean", 0.997},
        BenchmarkMetric{"rgb_mae", "1", "normalized", "mean", 0.002},
    };
    manifest.artifact_sha256 = vektoryum::ml::sha256_hex(artifact);
    return manifest;
}

}  // namespace

int main() {
    using namespace vektoryum::training;

    const std::vector<std::uint8_t> artifact = artifact_bytes();
    const BenchmarkResultManifest baseline = valid_manifest(artifact);
    const BenchmarkResultValidation accepted = validate_benchmark_result(baseline, artifact);
    expect_true(accepted.ok(), "benchmark result with exact provenance is accepted");
    expect_true(accepted.artifact_bytes == artifact.size(), "artifact byte accounting is exact");

    const std::string report_a = canonical_benchmark_report(baseline);
    const std::string report_b = canonical_benchmark_report(baseline);
    expect_true(report_a == report_b, "canonical benchmark report is byte deterministic");
    expect_true(report_a.find("metric=alpha_iou@1|ratio|mean|") != std::string::npos,
                "canonical report records metric version unit and aggregation");

    BenchmarkResultManifest invalid = baseline;
    invalid.metrics[0].unit.clear();
    expect_true(validate_benchmark_result(invalid, artifact).error ==
                    BenchmarkResultError::MissingMetricMetadata,
                "missing metric metadata is rejected");

    invalid = baseline;
    invalid.metrics[1].name = invalid.metrics[0].name;
    expect_true(validate_benchmark_result(invalid, artifact).error ==
                    BenchmarkResultError::DuplicateMetricName,
                "duplicate metric names are rejected");

    invalid = baseline;
    const BenchmarkMetric first = invalid.metrics[0];
    invalid.metrics[0] = invalid.metrics[1];
    invalid.metrics[1] = first;
    expect_true(validate_benchmark_result(invalid, artifact).error ==
                    BenchmarkResultError::NonDeterministicMetricOrder,
                "non-canonical metric ordering is rejected");

    invalid = baseline;
    invalid.metrics[0].value = std::numeric_limits<double>::infinity();
    expect_true(validate_benchmark_result(invalid, artifact).error ==
                    BenchmarkResultError::NonFiniteMetricValue,
                "non-finite benchmark values are rejected");

    invalid = baseline;
    invalid.evaluated_samples = 0U;
    expect_true(validate_benchmark_result(invalid, artifact).error ==
                    BenchmarkResultError::ZeroEvaluatedSamples,
                "zero evaluated samples are rejected");

    BenchmarkResultLimits limits;
    limits.max_evaluated_samples = 64U;
    expect_true(validate_benchmark_result(baseline, artifact, limits).error ==
                    BenchmarkResultError::SampleBudgetExceeded,
                "benchmark sample budget is fail-closed");

    limits = BenchmarkResultLimits{};
    limits.max_artifact_bytes = 4U;
    expect_true(validate_benchmark_result(baseline, artifact, limits).error ==
                    BenchmarkResultError::ArtifactBudgetExceeded,
                "benchmark artifact budget is fail-closed");

    std::vector<std::uint8_t> tampered = artifact;
    tampered[0] ^= 0x01U;
    expect_true(validate_benchmark_result(baseline, tampered).error ==
                    BenchmarkResultError::ArtifactDigestMismatch,
                "tampered benchmark artifact is rejected");

    invalid = baseline;
    invalid.artifact_sha256 = "not-a-sha256";
    expect_true(validate_benchmark_result(invalid, artifact).error ==
                    BenchmarkResultError::InvalidArtifactDigest,
                "malformed benchmark artifact digest is rejected");

    expect_true(validate_benchmark_result(baseline, {}).error == BenchmarkResultError::EmptyArtifact,
                "missing benchmark artifact is rejected");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All benchmark contract tests passed\n";
    return EXIT_SUCCESS;
}
