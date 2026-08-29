# Stage 8 — Training/data/benchmark pipeline

Weight: **7%**

Status: active foundation; do not count this stage complete until every acceptance item is implemented and the exact latest PR head passes all required CI with zero blocking review threads.

## Required contracts

- Deterministic dataset manifests with stable ordering and explicit schema/version identifiers.
- Immutable sample provenance including source identity, content digest, exact license identifier/terms, rights holder or grant source, and production-training usage authorization; missing, unknown, incompatible, or unverifiable license/rights metadata must fail closed.
- Dataset/manifests must preserve a deterministic, auditable binding between each sample digest and its license/rights provenance so artifact copies or renamed sources cannot silently inherit authorization.
- Deterministic train/validation/test split assignment from project-owned rules.
- Fail-closed duplicate/leakage detection across splits.
- Bounded preprocessing and benchmark resource budgets.
- Fail-closed malformed, missing, non-finite, provenance-invalid, license-invalid, or rights-unauthorized samples.
- Reproducible benchmark execution with explicit runtime/model/dataset/config identity.
- Explicit metric names, versions, parameters, units, and aggregation rules.
- Deterministic result manifests suitable for exact regression comparison.
- Regression/adversarial fixtures covering duplicate samples, split leakage, malformed metadata, digest mismatch, missing/unknown/incompatible license or rights provenance, unauthorized production-training use, digest-to-rights binding mismatch, resource overflow, and nondeterministic ordering.
- Existing source hygiene, Ubuntu, Windows, macOS, and Linux ASan/UBSan gates remain mandatory and may not be weakened.

## Merge acceptance

Stage 8 may be merged only when all required implementation and regression coverage is present on the exact latest head, all required checks are successful, mergeability is clean, and unresolved blocking review threads are zero.
