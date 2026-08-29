# Stage 11 — Quality/performance certification

Weight: **6%**

Status: active; do not count this stage complete until deterministic quality/performance evidence, exact provenance, bounded resources, adversarial regressions, exact-head CI, clean mergeability and zero blocking review threads satisfy every acceptance item.

## Required contracts

- Fixed, explicit quality and performance metric definitions with immutable acceptance thresholds.
- Exact input/output/toolchain provenance binding for every certificate.
- Fail-closed nonfinite, incomplete, stale, reordered, duplicated or threshold-violating evidence.
- Bounded sample counts, execution work, memory and benchmark artifact resources.
- Deterministic canonical certificate serialization and repeatable certificate identity.
- End-to-end certification over canonical Stage 0-10 outputs without weakening prior topology, alpha, provenance, exporter or sanitizer gates.
- Adversarial quality/performance fixtures, including near-threshold values, malformed evidence and provenance substitution.
- Existing source hygiene, Ubuntu, Windows, macOS and Linux ASan/UBSan gates remain mandatory.

## Implemented slices

The initial quality-certificate request contract requires schema/certificate identity, canonical lowercase input/output SHA-256 provenance, an explicit toolchain revision, bounded sample/execution budgets and a deterministically ordered metric-gate list. Metric values and bounds must all be finite, metric names must be unique and strictly ordered, and every measured value must remain within its immutable minimum/maximum gate. Validation fails closed on malformed digest provenance, missing toolchain identity, zero/excess resource evidence, empty/oversized metric sets, nonfinite metrics, duplicates, nondeterministic order and any threshold violation. Canonical request reporting uses locale-independent round-trippable floating-point serialization and rejects report-delimiter injection. Dedicated adversarial CTest coverage is wired into normal cross-platform tests and Linux sanitizer builds.

Certification validation is now anchored to the exact Stage 10 export chain instead of accepting free-form digest strings. The validator receives the Stage 10 ExportRequest, HybridOutputManifest and EncodedExportArtifact, first requires the artifact to pass the existing Stage 10 fail-closed validator, then requires the certificate input digest to equal the artifact source-output digest and the certificate output digest to equal the artifact's verified output digest. Adversarial regressions reject source-digest substitution, output-digest substitution and a tampered Stage 10 artifact before any quality evidence can be accepted.

Validated certification requests can now be issued as deterministic certificate artifacts. The artifact preserves certificate/input/output/toolchain provenance, stores the exact canonical certificate bytes and computes a canonical lowercase SHA-256 digest over those bytes using the repository digest implementation. Repeated issuance from the same validated Stage 10 artifact and certificate evidence must produce byte-identical certificate artifacts and identical digests; invalid provenance cannot issue an artifact. This digest is suitable as the stable evidence identity that later signing/release stages can bind without changing the quality gates themselves.

Concrete canonical-export measurement is now derived from validated Stage 10 evidence instead of caller-supplied resource claims. The measurement path first revalidates the exact encoded export artifact, then deterministically derives pixel sample count, encoder execution units, peak intermediate memory, exact output byte count and the hybrid seam error. The fixed metric set is strictly ordered as `export_bytes`, `peak_intermediate_bytes`, `seam_error`, `work_units`; each value is checked against the already-established export/output/resource budgets and the immutable seam-error ceiling. Repeated measurement of the same canonical artifact must produce identical metric evidence, while tampered artifacts and excessive seam evidence fail closed. The existing Stage 11 CTest target exercises these cases and therefore remains covered by cross-platform CI and Linux sanitizers.

User-facing image/vector quality measurements are now derived from bounded canonical comparison fixtures instead of caller-supplied metric values. A fixture binds equal-length reference/candidate alpha samples and reference/candidate binary vector-coverage masks. `alpha_mae` is computed from the exact byte deltas and retains the immutable `<= 0.02` ceiling; `vector_iou` is computed from exact binary-mask intersection/union and retains the immutable `>= 0.99` floor. Empty, size-mismatched, non-binary or over-budget fixtures fail closed. Repeated measurement of the same fixture is required to return identical ordered metric evidence, and adversarial regressions prove excessive alpha error and sub-threshold vector IoU are rejected. This remains inside the existing cross-platform CTest and Linux sanitizer target.

## Remaining acceptance

Material work remains: broaden repeatability across multiple canonical Stage 10 formats/quality fixtures; and add adversarial end-to-end certification scenarios that combine measured image/vector quality, measured export performance, certificate issuance and provenance substitution into one fail-closed chain.
