# Post-v0.1 benchmark contract

Status: frozen contract version 1.0.0. This is post-v0.1 measurement
infrastructure, not a v0.1 performance claim. Changing a workload, metric,
threshold, or measurement rule requires a new contract version and fresh
baselines.

## Scope and invariants

The harness measures the committed `portable_checked` generated code only. It
does not change decoder, framing, compiler, event, or generated-header
semantics. Benchmark options and warning flags are private to
`feedforge_benchmark`; benchmarks are disabled by default and are not installed
or exported.

Every timed process first verifies all 23 reviewed ITCH fixtures against both
canonical decoders, checks exact selected/skipped counts, checks complete
BinaryFILE replay counters, and checks strict rejection of bytes after the
zero-length end marker. A comparison is invalid unless the normal conformance
suite also passes for both builds.

## Frozen corpus and workloads

The public corpus is loaded at process startup from
`tests/fixtures/itch50/01_*.toml` through `23_*.toml`, in numeric order. The
harness accepts only format version 1 fixtures with an independent reviewer,
`review_status = "approved"`, and a `byte_source` stating that `raw_hex` is
hand-authored and not schema-generated. No schema-derived payload enters a
timed workload.

The workload sets are:

- `all_types`: all 23 payloads once in fixture order. It is used by
  `itch50_all`, where every known type is selected.
- `selected`: `A,F,E,C,X,D,U,P`, once each in that order. These are the eight
  events selected by `itch50_order_events`.
- `unselected`: `S,R,H,Y,L,V,W,K,J,h,Q,B,I,N,O`, once each in fixture order.
- `mixed`: all 23 payloads once in fixture order, giving eight selected and
  fifteen known-unselected messages for `itch50_order_events`.

The eight frozen cases are generated `decode_one` and in-memory strict
BinaryFILE replay for `itch50_all/all_types`, plus both operations for each of
`itch50_order_events/selected`, `unselected`, and `mixed`. BinaryFILE inputs
contain a big-endian two-byte length before every payload and one final
zero-length end marker.

Corpus SHA-256 is calculated over the contract tag followed by, for every
fixture, a big-endian 32-bit filename length, filename bytes, big-endian 32-bit
payload length, payload bytes, and one selected/unselected byte. Workload
hashes additionally preserve payload boundaries; replay hashes cover the exact
framed byte stream. The hashes and per-fixture hashes are result fields.

This corpus provides deterministic type and branch coverage. It is not a claim
about production message-frequency distributions.

## Measurement procedure

The comparison build is Release, C++20, IPO off, and has no added ISA or
`-march=native` flags. Exact compiler identity, compiler path and version,
configuration flags, target flags, generator, build type, generated pipeline
fingerprints, OS, kernel, architecture, and hardware manifest are written to
each result artifact.

Fixture I/O, TOML extraction, allocations, framing, SHA-256, host discovery,
decoder/sink construction, and correctness checks occur before a measurement
clock starts. Timed replay intentionally includes construction performed by
the public `replay_binary_file` function itself. No file I/O occurs in a timed
region.

Each case is calibrated from the configured batch floor until one sample lasts
at least the configured minimum time. It then executes the configured warm-up
count and recorded sample count at one fixed rounds-per-sample value.
`std::chrono::steady_clock` is mandatory. Defaults are 256 batch rounds, five
warm-ups, fifteen samples, and 50 ms minimum sample time. Smoke mode uses
smaller values and is never comparison-eligible.

The sink consumes every emitted event through a compiler barrier and updates a
deterministic checksum. Decode outcomes and replay summaries also feed the
checksum. Checksums must be stable across samples. This sink cost is part of
the frozen measurement; results do not represent a sink-free decoder.

For every sample, artifacts contain elapsed nanoseconds and exact bytes,
messages, events, rounds, and checksum. Decode byte rates count payload bytes;
replay byte rates count prefixes and the end marker. Summary metrics include
median, p05, p95, minimum, maximum, and median absolute deviation (MAD) for
sample time, ns/message, bytes/second, messages/second, and, when nonzero,
ns/event and events/second.

A run is implausible and must be discarded if a sample falls below 75% of its
configured minimum duration, the median is below 0.01 ns/message, a checksum or
counter changes, or the clock is unusable. It is noisy if within-run normalized
MAD exceeds 5% or the p95-p05 span exceeds 20% of the median. The harness emits
these diagnostics; it never converts one process run into a claim.

## Optimization acceptance

Targets must be named before implementation. Baseline and candidate must use
the same contract, corpus hashes, correctness checksum, hardware, OS/kernel,
compiler and flags, build configuration, sample settings, and case counters.
Capture them in the same quiet session; alternate baseline and candidate
process order when practical to reduce thermal and time-order bias.

An optimization is a win only when all of these predeclared rules hold:

1. Both normal test suites and all harness correctness checks pass.
2. Each side has at least seven independent process runs, at least eleven
   samples per case per run, and at least 50 ms per sample.
3. No retained run is implausible or noisy, and each case has cross-run
   MAD/median at most 3%.
4. Every predeclared target improves median ns/message by at least 5%.
5. For each target, `improvement - 1.4826 * (baseline normalized MAD +
   candidate normalized MAD)` is at least 3%.
6. No other frozen case regresses in median ns/message by more than 2%.
7. The candidate confirms directionally on the holdout corpus described below.

`benchmarks/benchmark.py compare` applies rules 2 through 6 after checking
artifact identity and correctness. Its nonzero result means “not demonstrated,”
not necessarily “the implementation is slower.”

## Hardware manifest and reproducibility

A result must record CPU and machine model, logical and physical CPU counts,
memory, OS and kernel, architecture, process affinity, governor/power policy and
turbo/boost state when exposed, compiler/version/path/flags, build type,
command/configuration, corpus hash, and UTC timestamp. Missing fields and
platform limitations remain explicit in the artifact.

On Linux, use an otherwise idle host, AC power, a fixed performance policy when
permitted, and pin to one identified physical core, for example:

```sh
taskset -c 2 build/bench/benchmarks/feedforge_benchmark
```

Record whether SMT shares that core and preserve the reported affinity,
governor, and boost state. Container CPU quotas and virtualized hosts need
separate disclosure.

macOS has no supported process CPU-affinity API, and Apple Silicon scheduling
may move work between heterogeneous cores. The harness records this limitation
and cannot capture frequency, turbo, power mode, or thermal pressure. Run on AC
power with Low Power Mode disabled, close competing work, allow thermal
stabilization, and reject noisy series. A local macOS series is useful as a
baseline but does not erase these limitations.

## Holdout policy and public claims

The holdout is a legally usable capture or synthetic set maintained outside
the repository, identified only by immutable SHA-256, provenance, framing
status, and message-type counts in private run metadata. Optimizers may not
inspect, repartition, regenerate, or tune against it. Run it only after a
candidate passes the public corpus. A holdout failure rejects the candidate;
it does not authorize changing this public contract or silently replacing the
holdout.

No README performance statement may be based on a single run, smoke mode,
public fixtures alone, unmatched machines, or a cherry-picked case. A future
README claim requires a passing comparison plus holdout confirmation and must
state the exact operation/pipeline, baseline and candidate revisions, hardware,
compiler and flags, corpus class, statistic, repeat count, uncertainty/noise
bound, and a durable link to JSON/CSV artifacts. It must not generalize a
fixture result into exchange-feed, latency, production, or cross-platform
claims.

## Commands and artifacts

Configure, build, and run the opt-in smoke test:

```sh
cmake --preset bench
cmake --build --preset bench
ctest --preset bench
```

Collect a seven-process local baseline under the ignored build tree:

```sh
python3 benchmarks/benchmark.py run \
  --executable build/bench/benchmarks/feedforge_benchmark \
  --output-dir build/bench/results/baseline \
  --label baseline \
  --source-id <commit-or-patch-id> \
  --correctness-command "ctest --preset bench"
```

Compare two complete series, naming targets that were declared before the
optimization:

```sh
python3 benchmarks/benchmark.py compare \
  --baseline build/bench/results/baseline/series.json \
  --candidate build/bench/results/candidate/series.json \
  --target decode_one/itch50_order_events/mixed \
  --output-json build/bench/results/comparison.json \
  --output-csv build/bench/results/comparison.csv
```

The executable writes human-readable output and fixed-order UTF-8 CSV plus
canonical, sorted/minified JSON. Result timestamps and machine data exist only
in result artifacts under ignored build directories; they never enter
generated headers. The repeat runner additionally records a caller-supplied
source identifier and SHA-256 of the exact benchmark executable.

Current limitations are deliberate: there are no hardware performance
counters, cache flushing, real-feed frequency model, thread-contention model,
or macOS affinity control. The fixture parser supports only the reviewed fields
needed by this contract rather than general TOML.
