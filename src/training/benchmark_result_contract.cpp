#include "vektoryum/training/benchmark_result_contract.hpp"

#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

#include "vektoryum/ml/artifact_digest.hpp"
#include "vektoryum/training/dataset_contract.hpp"

namespace vektoryum::training {
namespace {
[[nodiscard]] BenchmarkResultValidation fail(BenchmarkResultError error, std::size_t index, std::uint64_t bytes) noexcept { return BenchmarkResultValidation{error,index,bytes}; }
[[nodiscard]] bool missing_identity(std::string_view a, std::string_view b) noexcept { return a.empty() || b.empty(); }
}  // namespace

std::string canonical_benchmark_report(const BenchmarkResultManifest& m) {
    std::ostringstream out; out.imbue(std::locale::classic());
    out << "schema=" << m.schema_version << '\n' << "benchmark=" << m.benchmark_id << '@' << m.benchmark_version << '\n'
        << "run=" << m.run_id << '\n' << "dataset=" << m.dataset_id << '@' << m.dataset_version << '\n'
        << "model=" << m.model_id << '@' << m.model_version << '\n'
        << "architecture_revision=" << m.architecture_revision << '\n'
        << "training_code_revision=" << m.training_code_revision << '\n'
        << "degradation_pipeline_revision=" << m.degradation_pipeline_revision << '\n'
        << "training_artifact_sha256=" << m.training_artifact_sha256 << '\n'
        << "runtime=" << m.runtime_id << '@' << m.runtime_version << '\n' << "seed=" << m.seed << '\n'
        << "evaluated_samples=" << m.evaluated_samples << '\n';
    out << std::scientific << std::setprecision(17);
    for (const BenchmarkMetric& metric : m.metrics) out << "metric=" << metric.name << '@' << metric.version << '|' << metric.unit << '|' << metric.aggregation << '|' << metric.value << '\n';
    return out.str();
}

BenchmarkResultValidation validate_benchmark_result(const BenchmarkResultManifest& m, const std::vector<std::uint8_t>& artifact, const BenchmarkResultLimits& limits) {
    const auto bytes=static_cast<std::uint64_t>(artifact.size());
    if(m.schema_version.empty()) return fail(BenchmarkResultError::MissingSchemaVersion,0,bytes);
    if(missing_identity(m.benchmark_id,m.benchmark_version)) return fail(BenchmarkResultError::MissingBenchmarkIdentity,0,bytes);
    if(m.run_id.empty()) return fail(BenchmarkResultError::MissingRunIdentity,0,bytes);
    if(missing_identity(m.dataset_id,m.dataset_version)) return fail(BenchmarkResultError::MissingDatasetIdentity,0,bytes);
    if(missing_identity(m.model_id,m.model_version)) return fail(BenchmarkResultError::MissingModelIdentity,0,bytes);
    if(m.architecture_revision.empty()) return fail(BenchmarkResultError::MissingArchitectureRevision,0,bytes);
    if(m.training_code_revision.empty()) return fail(BenchmarkResultError::MissingTrainingCodeRevision,0,bytes);
    if(m.degradation_pipeline_revision.empty()) return fail(BenchmarkResultError::MissingDegradationPipelineRevision,0,bytes);
    if(m.training_artifact_sha256.empty()) return fail(BenchmarkResultError::MissingTrainingArtifactChecksum,0,bytes);
    if(!is_sha256_hex(m.training_artifact_sha256)) return fail(BenchmarkResultError::InvalidTrainingArtifactChecksum,0,bytes);
    if(missing_identity(m.runtime_id,m.runtime_version)) return fail(BenchmarkResultError::MissingRuntimeIdentity,0,bytes);
    if(m.evaluated_samples==0) return fail(BenchmarkResultError::ZeroEvaluatedSamples,0,bytes);
    if(m.evaluated_samples>limits.max_evaluated_samples) return fail(BenchmarkResultError::SampleBudgetExceeded,0,bytes);
    if(m.metrics.empty()) return fail(BenchmarkResultError::EmptyMetrics,0,bytes);
    if(m.metrics.size()>limits.max_metrics) return fail(BenchmarkResultError::TooManyMetrics,0,bytes);
    std::string_view prev;
    for(std::size_t i=0;i<m.metrics.size();++i){ const auto& metric=m.metrics[i];
        if(metric.name.empty()||metric.version.empty()||metric.unit.empty()||metric.aggregation.empty()) return fail(BenchmarkResultError::MissingMetricMetadata,i,bytes);
        if(!std::isfinite(metric.value)) return fail(BenchmarkResultError::NonFiniteMetricValue,i,bytes);
        if(!prev.empty()){ if(metric.name==prev) return fail(BenchmarkResultError::DuplicateMetricName,i,bytes); if(metric.name<prev) return fail(BenchmarkResultError::NonDeterministicMetricOrder,i,bytes); } prev=metric.name;
    }
    if(!is_sha256_hex(m.artifact_sha256)) return fail(BenchmarkResultError::InvalidArtifactDigest,0,bytes);
    if(artifact.empty()) return fail(BenchmarkResultError::EmptyArtifact,0,0);
    if(bytes>limits.max_artifact_bytes) return fail(BenchmarkResultError::ArtifactBudgetExceeded,0,bytes);
    if(ml::sha256_hex(artifact)!=m.artifact_sha256) return fail(BenchmarkResultError::ArtifactDigestMismatch,0,bytes);
    return BenchmarkResultValidation{BenchmarkResultError::None,m.metrics.size(),bytes};
}
}  // namespace vektoryum::training
