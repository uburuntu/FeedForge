# Testing

This guide defines how FeedForge tests protocol conformance, generated code,
runtime behavior, packaging, and supported platforms. FeedForge is experimental,
is not exchange-certified, and is not production trading infrastructure.

A test result applies only to the exact source revision, configuration,
toolchain, and platform that produced it. The presence of a preset, fixture
manifest, corpus, or target does not establish coverage unless the corresponding
cases are built and run.

## Test independence and fixture provenance

Fixture bytes and expected values must be hand-authored from the official
protocol document or independently reviewed. They must not all be derived from
the schema, emitter, or load functions being tested; that would only prove that
one implementation agrees with itself.

Every protocol fixture records:

- the official source URL, revision, and relevant page or table;
- reviewed raw hexadecimal bytes;
- expected message identity and decoded field values;
- the expected event under each canonical pipeline that selects it;
- the review date and approval status; and
- the structured `independent-line-by-line-protocol-review` provenance marker.

Independent review is a distinct pass from fixture construction. It checks raw
bytes, offsets, widths, values, and expected events directly against the cited
official table, not merely against generated output. A binary fixture may be
derived from the reviewed hex by a small utility, but the reviewed hex remains
the source of truth. The provenance marker records that process and does not
identify an individual.

Authoritative source URLs, retrieval dates, document revisions, and SHA-256
checksums belong in [`schemas/sources.lock.toml`](../schemas/sources.lock.toml).
If a mutable upstream document changes checksum, stop for human review; do not
automatically regenerate fixtures. For protocol facts, the precedence order is
the official document, independently reviewed fixtures, then implementation.
FeedForge-specific schema and pipeline behavior is defined by the
[schema format](schema-format.md) and [pipeline format](pipeline-format.md).

## Required test layers

### Runtime primitives

Unit tests cover big-endian loads of widths 1, 2, 4, 6, and 8 with zero,
all-ones, alternating, and boundary values; 48-bit timestamps; fixed-point raw
preservation; ASCII padding and non-allocating trailing-space trimming;
strong-identifier distinctions; flow/result helpers; event traits; and
feature-detection paths in both C++20 and C++23.

### Schema, pipeline, FFIR, and emitter

Each rule in the [schema format](schema-format.md) and
[pipeline format](pipeline-format.md) needs a focused positive or negative
test. Coverage includes exact identifier acceptance and rejection, C++ keywords
and scope collisions, safe source emission, stable diagnostic codes and paths,
canonical FFIR JSON, and generated C++ compilation in C++20 and C++23.

Determinism tests compare semantic fingerprints, FFIR, and generated source
after changing comments, non-semantic TOML key or table order, working
directory, and repeated invocation. Two independent canonical-generation runs
must have the same SHA-256. Diagnostics and output are checked for absolute
paths, timestamps, host or user data, and other unstable provenance.

### BinaryFILE framing

Framing tests cover empty incomplete and empty complete sessions; one and
multiple frames; complete and incomplete termination; the zero marker after
frames; one-byte prefixes; truncated payloads; maximum 16-bit payload length;
trailing bytes under strict replay; offsets and ordinals; exact `consumed()` and
`remaining()` values; sticky terminal outcomes; and cooperative replay stop.

The cursor and strict generated replay are separate layers: the cursor exposes
bytes after an end marker, while strict replay classifies them as
`trailing_data_after_end_marker`.

### ITCH conformance

For every message in the canonical
[`totalview_itch_5_0.toml`](../schemas/nasdaq/totalview_itch_5_0.toml) schema,
independently reviewed coverage includes:

- at least one valid all-fields fixture;
- successful `itch50_all` decode with exact values;
- exact rejection at declared size minus one and plus one; and
- a valid skip under `itch50_order_events` when unselected.

The order-events pipeline also requires exact emission for `A`, `F`, `E`, `C`,
`X`, `D`, `U`, and `P`. Cross-message tests cover lowercase `h` versus uppercase
`H`, message `O`, unknown discriminators under both policies, empty payloads,
complete and incomplete multi-frame input, malformed known-unselected messages,
sink stop, and exact replay counters.

### Projection and compile-fail

Compile-time and emitted-source tests prove that:

- an event has no unrequested member;
- generated code emits no load for an unrequested field;
- every selected event requires a valid sink overload;
- missing, ambiguous, throwing, and wrong-result overloads fail clearly;
- event layout is unchanged between C++20 and C++23 on the same ABI and
  configuration; and
- errors, skips, stops, and sink ordering do not depend on profile internals.

An instrumented `decoder_implementation` counts `load_unsigned` calls. Empty,
unknown, undersized, oversized, and malformed known-unselected payloads must
perform zero field loads. This proves that exact-size validation precedes field
access.

### Replay, packaging, and consumers

End-to-end tests frame reviewed payloads, decode them through generated
pipelines, and verify event order, terminal state, counters, consumed bytes, and
error offsets. A separate project installs FeedForge to a temporary prefix,
uses `find_package`, generates a pipeline with the installed compiler, builds
the consumer as C++20, and runs it. Runtime-only minimum-compiler jobs disable
the host compiler and consume committed canonical headers instead.

## Allocation, exceptions, and realtime checks

`hardening.allocation` finishes input and sink setup, resets replacements for
the scalar, array, aligned, and nothrow global allocation functions, and then
measures only the decode or replay call. Its fixed-storage sink does not
allocate. The test separately covers emitted, skipped, complete, incomplete,
and sink-stopped paths. This isolates FeedForge-owned work; user sinks remain
responsible for their own behavior.

`feedforge_test_no_exceptions_rtti` is an explicit C++20 target that compiles and
runs the committed all-message decoder and replay adapter with
`-fno-exceptions -fno-rtti`. Hot entry points and sink calls are also checked as
`noexcept`.

The `rtsan` preset probes both the `[[clang::nonblocking]]` attribute and a
linkable `-fsanitize=realtime` runtime. Only a successful upstream-Clang probe
creates `hardening.rtsan_smoke`; unsupported compilers report that the optional
test is disabled and continue configuring. RTSan is not a minimum-toolchain
requirement, and its private compile and link options are never attached to
exported runtime or generated interface targets.

Test configuration also inspects exported runtime and canonical target
properties and fails if project warning or sanitizer compile and link options
leak through an interface.

## Sanitizers

The `sanitizers` preset builds the applicable tests with Clang
AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
cmake --preset sanitizers
cmake --build --preset sanitizers
ctest --preset sanitizers
```

Leak detection is enabled in CI where supported. A suppression must not hide a
FeedForge project-code defect. Sanitizer success complements focused boundary
tests; it does not replace validation-order or fixture assertions.

## Fuzzing

The Clang libFuzzer executables are:

1. `fuzz_binary_file`: arbitrary bytes through `binary_file_cursor`;
2. `fuzz_decode_one`: arbitrary payload through the canonical
   `itch50_all::basic_decoder`; and
3. `fuzz_replay`: arbitrary BinaryFILE through canonical strict replay and a
   complete `noexcept` no-op sink.

At configure time, `fuzz/generate_corpus.cmake` validates `raw_hex` against
`raw_size` and derives isolated build-tree seeds from all reviewed fixtures. The
reviewed TOML remains the source of truth. Committed error seeds cover empty
payload, unknown type, invalid size, complete and incomplete framing, truncated
prefix or payload, and trailing data. The generated manifest and aggregate
replay make the mapping traceable without host-dependent paths or parallel
build writes.

For identical bytes, the targets assert deterministic outcomes, no sink call
after an error, at most one sink call per payload, validation before counted
field loads, and coherent replay counters and terminal fields. ASan, UBSan, and
libFuzzer supply crash, undefined-behavior, bounds, hang, and allocation stress
around those semantic assertions.

CI runs a short fixed-duration smoke over the seeds. Longer fuzz campaigns are
scheduled or manual, preserve useful reproducers, and record the toolchain,
target, corpus revision, duration, and failing input. Configure and build the
fuzzers with:

```sh
cmake --preset fuzz
cmake --build --preset fuzz
```

The fuzz preset fails with an actionable diagnostic when the selected Clang has
no usable libFuzzer, ASan, and UBSan runtime. Normal test builds also compile the
same three harnesses with fixed deterministic arbitrary inputs as
`hardening.arbitrary_input.*`; this is the local smoke path for AppleClang
installations that omit libFuzzer.

## Local presets

Run the focused compiler and unit tests with:

```sh
cmake --preset fast
cmake --build --preset fast
ctest --preset fast
```

The `dev` preset runs the complete configured suite plus examples. The
`release` preset repeats the suite with release-mode checks active. Useful
focused gates are:

```sh
cmake --preset compiler-off
cmake --build --preset compiler-off
ctest --preset compiler-off

cmake --preset no-exceptions-rtti
cmake --build --preset no-exceptions-rtti
ctest --preset no-exceptions-rtti

ctest --test-dir build/dev -L allocation --output-on-failure
```

Preset definitions are maintained in
[`CMakePresets.json`](../CMakePresets.json). Warnings are errors for project and
generated sources in CI. Hosted CI is correctness infrastructure and must not
publish latency or throughput claims.

## CI and platform policy

The [main CI workflow](../.github/workflows/ci.yml) must cover:

- minimum supported GCC and Clang C++20 runtime and generated-code builds with
  the host compiler disabled;
- supported GCC and Clang full builds, including generated-code C++23
  compatibility;
- Clang ASan and UBSan, no-exception and no-RTTI, release-mode, installed
  generation, and optional RTSan checks;
- Linux x86-64 Tier 1, macOS arm64 AppleClang Tier 2, and Windows x64 MSVC 2022
  Tier 2; and
- the separate [Linux fuzz-smoke workflow](../.github/workflows/fuzz-smoke.yml)
  for all three libFuzzer targets.

Tier 1 failures block integration. Tier 2 jobs are expected to pass and must not
be silently marked `continue-on-error`. A temporary Tier 2 exception must be
explicit, time-bounded, and tracked with the affected platform and toolchain,
failure analysis, last-known-green revision, user impact, owner, and restoration
plan. Tier 1 and fuzz gates cannot be waived.
