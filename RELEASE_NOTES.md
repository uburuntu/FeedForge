# FeedForge v0.6.0

FeedForge v0.6.0 adds a frozen 16-case benchmark contract for one-shot and
caller-buffered chunked replay, together with source, executable, artifact, and
host-state provenance gates. FeedForge remains experimental, is not
exchange-certified, and is not production trading infrastructure.

## Highlights

- Benchmark contract 2.0.0 retains the eight contract 1 decode and strict
  replay cases and adds eight chunked replay cases across the same four
  pipeline/workload pairs.
- Frame-aligned schedules submit a complete prefix and payload per push;
  one-byte schedules exercise maximal fragmentation. Both call `finish()` once
  per round and bind their exact chunk sequence with SHA-256.
- Pre-timing correctness requires chunked and strict replay to agree in final
  summary, counters, event order, and sink checksum for every added case.
- The runner binds results to an exact clean 40-character Git revision and
  executable hash, requires Release C++20 with IPO and CPU-specific tuning off,
  and rejects case, corpus, checksum, counter, or configuration drift.
- Full-series validation reloads all seven raw run JSON files and reconstructs
  the aggregate. Diagnostic unqualified series require an explicit escape hatch
  and are not release evidence.
- Build-log redaction produces a separate public copy without changing raw
  timing data. Release instructions keep benchmark evidence checksums separate
  from deterministic source archive checksums.
- Required hosted CI runs deterministic 16-case smoke and Python mutation and
  artifact validation on Python 3.11. It does not publish hosted timing.

## Qualification and evidence

The v0.6 evaluation is a new contract baseline, not an optimization comparison.
Qualification requires exactly seven processes with 15 recorded samples and
five warm-ups per case, every sample meeting the 50 ms floor, no noisy or
implausible run, and cross-run MAD/median at most 3% for every case. The exact
candidate must also pass the complete local verification matrix.

Retained timing must run on an otherwise idle, cooled host on AC power. The
runner waits 120 seconds after its captured correctness gate. macOS evidence
records the pre/post battery source, power configuration, and thermal status and
preserves the platform's lack of supported process affinity as a limitation.

The GitHub release carries reviewed individual raw JSON and CSV, aggregate
series, redacted public correctness and qualification logs, pre/post host-state
records, the benchmark executable hash, and `BENCHMARK_SHA256SUMS`. The normal
`SHA256SUMS` continues to cover only the deterministic source `.tar.gz` and
`.zip` archives. Private holdout material, recursive build trees, credentials,
proprietary data, licensed captures, and unreviewed raw logs are excluded.

No numeric timing value is committed to the source tree. The release evidence
is content-addressed to the final v0.6.0 tag and retains its exact host,
toolchain, configuration, source, executable, corpus, and uncertainty context.

## Compatibility

The package and generated-header identity are `0.6.0`. Runtime API epoch 1 and
revision 0 are unchanged. Schema, pipeline, and FFIR format versions remain 1;
`portable_checked.v1` remains the only implementation profile. The synthetic
conformance bundle remains format version 1. Benchmark contract 2.0.0 is an
independent evidence format and does not change runtime decode semantics.

Installed CMake packages continue to use same-minor compatibility before 1.0;
a 0.5 request does not silently accept 0.6. Regenerating custom headers is
recommended so their compiler provenance records v0.6.0, although v0.5
generated headers remain compatible with runtime API epoch 1, revision 0.

Runtime and generated headers remain strict C++20. The host compiler requires
C++23 with GCC 13.2, Clang 17, or MSVC 19.38 and a corresponding standard
library, or newer. Python 3.11 or newer is required only for conformance bundle
and benchmark artifact tooling.

## Limitations

- The public benchmark corpus provides deterministic type and branch coverage,
  not production message frequencies. Frame-aligned and one-byte schedules are
  API stress shapes, not network packetization models.
- A qualified baseline does not establish an optimization win, absolute speed,
  live-feed latency, production throughput, cross-platform equivalence,
  exchange certification, or operational trading safety.
- The local vcpkg overlay remains checkout-scoped rather than a registry
  release. Pin the source checkout and vcpkg baseline and keep binary caching
  disabled when validating exact source bytes.
- FeedForge provides no live networking, packet recovery, sequencing, order
  book, strategy, capture service, database, or operational trading controls.

See the [v0.5.0 GitHub Release](https://github.com/uburuntu/FeedForge/releases/tag/v0.5.0)
for the previous release notes and validation evidence.
