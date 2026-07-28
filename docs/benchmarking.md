# Benchmarking

This document defines evaluation contract version 2.0.0. Benchmark results are
engineering evidence, not a product performance claim. Changing a workload,
schedule, metric, threshold, or measurement rule requires a new contract version
and fresh baselines. The archived [contract 1.0.0](benchmarking-v1.md) remains
the authority for the v0.1 optimization case study.

## Scope and invariants

The harness measures only committed `portable_checked` generated code. It does
not change decoder, framing, compiler, event, or generated-header semantics.
Benchmark options and warning flags are private to `feedforge_benchmark`;
benchmarks are disabled by default and are not installed or exported.

Every process verifies all 23 reviewed ITCH fixtures against both canonical
decoders before timing. It checks exact selected and skipped counts, strict
BinaryFILE replay, rejection after the end marker, and all eight chunked cases.
For each chunked case the final summary, counters, event count, event order, and
sink checksum must equal one-shot strict replay. A timing series is invalid
unless the normal conformance suite passes at the same exact source revision.

## Frozen corpus and workloads

The corpus is loaded from `tests/fixtures/itch50/01_*.toml` through
`23_*.toml`, in numeric order. Only format version 1 fixtures with approved
independent review and hand-authored `raw_hex` are accepted. No schema-derived
payload enters a timed workload.

The workload sets are:

- `all_types`: all 23 payloads in fixture order, with 694 payload bytes and
  742 framed bytes;
- `selected`: the eight `A,F,E,C,X,D,U,P` order events, with 264 payload bytes
  and 282 framed bytes;
- `unselected`: the other 15 known messages, with 430 payload bytes and
  462 framed bytes; and
- `mixed`: all 23 payloads through `itch50_order_events`, producing eight
  events and 15 known-message skips.

Contract 2.0.0 contains exactly 16 ordered cases. It retains `decode_one` and
strict `replay_binary_file` for `itch50_all/all_types` and for the order-event
`selected`, `unselected`, and `mixed` workloads. It adds `chunked_replay` under
both schedules for those same four pipeline/workload pairs:

- `frame_aligned` submits one complete two-byte prefix plus payload per message,
  then the two-byte end marker. Its pushes per round are 24, 9, 16, and 24.
- `one_byte` submits every framed byte separately. Its pushes per round are
  742, 282, 462, and 742. This is a maximal-fragmentation stress shape, not a
  production traffic or packetization model.

Every chunked round calls `finish()` exactly once. Case IDs, order, operation,
pipeline, workload, byte/message/event counts, push and finish counts, workload
hashes, and schedule hashes are pinned in `benchmarks/benchmark.py`. The
schedule SHA-256 hashes the ASCII contract tag, schedule name, and ordered
decimal chunk sizes. The workload hash independently binds the input bytes.

The corpus supplies deterministic type and branch coverage. It is not a model
of production message frequencies.

## Measurement procedure

The qualification build is Release, strict C++20, IPO off, and has no added
ISA, `-march`, `-mcpu`, or `-mtune` flags. Collection requires a clean tracked
Git worktree and an exact lowercase 40-character commit SHA matching `HEAD`.
The executable embeds that revision and whether its configure-time source was
dirty. Each run records the exact executable SHA-256, compiler and flags,
generated fingerprints, OS/kernel, architecture, and host manifest.

Fixture I/O, TOML extraction, allocation, hashing, host discovery, and
correctness checks occur before measurement. Decode and strict replay retain
their contract 1.0 timed boundaries. For chunked replay, caller scratch is
prepared before the clock; replayer construction, every scheduled `push()`,
and `finish()` are timed for every round. No file I/O occurs in a timed region.

The sink consumes emitted events through a compiler barrier and updates an
ordered deterministic checksum. Final outcomes and counters also feed the
anti-elision checksum, which must be stable across samples.

Each case calibrates from a batch floor of 256 rounds. Qualification requires
exactly five warm-up samples and 15 recorded samples per process, with every
recorded sample meeting the 50 ms duration floor. `std::chrono::steady_clock`
is mandatory. Smoke mode uses smaller values and is never evidence-eligible.

Artifacts record elapsed time and exact bytes, messages, events, pushes,
finish calls, rounds, and checksums. Summary distributions include median,
p05, p95, minimum, maximum, and MAD for sample time, ns/message, byte and
message rates, optional event metrics, and ns/push for chunked cases.

A run is implausible and rejected if a duration, counter, checksum, clock, or
per-message timing gate fails. A run is noisy if within-run MAD/median exceeds
5% or the p95-p05 span exceeds 20% of median. These are rejection rules, not
permission to remove individual samples.

## v0.6 baseline qualification

The v0.6 evaluation is a new contract baseline, not an optimization comparison.
It has no absolute speed threshold. Cross-case or cross-schedule ratios are
descriptive only because operations and push counts differ. No result may be
described as an optimization win, feed latency, or production throughput.

Qualification has two phases:

1. At the exact candidate SHA, run `make verify-all` and preserve its complete
   log. This covers the portable matrix, sanitizers, seven fuzzers, generated
   output, conformance, formatting, and benchmark smoke.
2. Let the host return to a cooled idle state. Immediately before collection,
   `make bench-correctness` reruns the full dev CTest, conformance, generated
   gate, and deterministic benchmark smoke. The repeat runner captures this
   output and then waits the frozen 120-second idle cooldown before timing.

Retain exactly seven independent processes. Every run must be non-implausible
and non-noisy, and every one of the 16 cases must have cross-run MAD/median at
most 3%. `benchmark.py run` fails qualification by default; diagnostic output
requires the explicit `--allow-unqualified` escape hatch and is not releasable.
The aggregate's `qualification.qualified` field covers these frozen series
checks only. Release qualification additionally requires the phase-one
verification and pre/post host-state gates described here.

Do not select seven quiet runs from a larger attempt. If any gate fails, reject
the complete attempt, preserve its identity and reason, correct the environment,
and run seven fresh processes into a new empty directory. Thresholds and cases
must not change in response to results.

Use an otherwise idle host on AC power. On macOS, the AC profile must use frozen
Automatic/legacy-off power mode 0 and the machine must cool before collection;
macOS has no supported process
affinity API and may move work between heterogeneous cores. Preserve pre- and
post-series output for AC/battery state, power-mode configuration, and thermal
status, including `pmset -g batt`, `pmset -g custom`, and `pmset -g therm`. If
those conditions or records are unavailable, do not retain the timing series.
The endpoint snapshots cannot prove uninterrupted AC power during collection;
polling during timed samples is deliberately avoided because it can perturb them.

## Commands

Build and run the deterministic, non-timing smoke checks:

```sh
make bench-smoke
```

After phase-one verification and host cooldown, collect the exact candidate:

```sh
make bench-run \
  BENCH_LABEL=v0.6.0-qualified \
  BENCH_SOURCE_ID="$(git rev-parse HEAD)" \
  BENCH_COOLDOWN_SECONDS=120
```

Revalidate a retained series from all seven raw JSON files rather than trusting
only its aggregate:

```sh
python3 benchmarks/benchmark.py validate-series \
  --series build/bench/results/v0.6.0-qualified/series.json \
  --runs-dir build/bench/results/v0.6.0-qualified
```

The validator reloads every raw run, checks their paths and hashes, reconstructs
the aggregate, and rejects identity or readiness drift. Validate a single smoke
artifact with:

```sh
python3 benchmarks/benchmark.py validate \
  --artifact build/bench/benchmark-smoke.json \
  --csv build/bench/benchmark-smoke.csv
```

For a future same-contract optimization comparison, name every target
explicitly. Contract 1 and contract 2 artifacts are not comparable.

```sh
python3 benchmarks/benchmark.py compare \
  --baseline build/bench/results/baseline/series.json \
  --candidate build/bench/results/candidate/series.json \
  --target chunked_replay/one_byte/itch50_order_events/mixed \
  --output-json build/bench/results/comparison.json \
  --output-csv build/bench/results/comparison.csv
```

## Evidence and privacy

Preserve all seven raw `run-*.json` and `run-*.csv` files, all seven
capture-sanitized and hash-bound `run-*.txt` files, `series.json`, `series.csv`,
the capture-sanitized and hash-bound `correctness.txt`, the exact executable
hash, the phase-one verification log, and pre/post power and thermal logs. The
portable validator requires the exact JSON, CSV, and text files; do not rewrite
them.

The runner mechanically strips known credential forms, usernames, checkout
paths, temporary paths, and terminal controls before hashing human output. This
is not exhaustive and does not replace manual review. Separately captured build
logs, including the phase-one verification log, need a distinct public copy:

```sh
python3 benchmarks/benchmark.py redact-log \
  --input out/benchmark-local/v0.6.0/verify-all.txt \
  --output out/benchmark-evidence/v0.6.0/verify-all.public.txt \
  --source-root "$(pwd)"
```

Inspect every prospective asset for credentials, proprietary captures,
licensed exchange data, and unexpected absolute paths. Never upload the private
holdout, the entire build tree, or an unreviewed raw log.

Publish individual reviewed evidence files with a separate
`BENCHMARK_SHA256SUMS`. Keep it separate from release `SHA256SUMS`, which covers
only deterministic source archives. Timing evidence is host- and
session-specific even when its captured bytes are content-addressed. Download
all assets after publication, verify both checksum manifests, and compare every
downloaded byte with the local staged asset.

No generated timing data is committed. The committed v0.6 evaluation guide
defines the method and claim boundary; the release assets carry the observed
series. Current deliberate limitations include no hardware counters, cache
flushing, real-feed frequency model, thread-contention model, or macOS affinity
control.
