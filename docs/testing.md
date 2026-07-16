# Testing and release evidence

> **Status:** This document defines the required v0.1 test strategy from SPEC
> Sections 21, 22, and 26. It distinguishes release gates from tests that exist
> in the current development scaffold. FeedForge is experimental, is not
> exchange-certified, and is not production trading infrastructure.

This is a list of required evidence, not a claim that every test is present in
a particular checkout. The existence of a preset, fixture manifest, corpus, or
target does not establish coverage by itself: the release audit must record
that all required cases were independently reviewed, built, and run. Passing a
partial development suite is not v0.1 release evidence.

## Test independence and fixture provenance

Fixture bytes and expected values must be hand-authored from the official
protocol specification or independently reviewed. They must not all be derived
from the schema, emitter, or load functions being tested; that would only prove
that one implementation agrees with itself.

Every protocol fixture records:

- official specification URL/revision and section/page;
- reviewed raw hexadecimal bytes;
- expected message identity and decoded field values;
- expected event under each canonical pipeline that selects it;
- author identity; and
- independent reviewer identity.

Author and reviewer must be different human or worker identities. The reviewer
checks raw bytes, offsets, widths, values, and expected events directly against
the cited official table, not merely against generated output. A binary fixture
may be derived from the reviewed hex by a tiny utility, but the reviewed hex
remains the source of truth.

Authoritative source URLs, retrieval dates, document revisions, and SHA-256
checksums belong in `schemas/sources.lock.toml`. If a mutable upstream document
changes checksum, stop for human review; do not automatically regenerate
fixtures. The precedence order is official document, explicit
[SPEC.md](../SPEC.md) decisions, audited fixtures, then implementation.

## Required test layers

### Runtime primitives

Unit tests cover big-endian loads of widths 1, 2, 4, 6, and 8 with zero,
all-ones, alternating, and boundary values; 48-bit timestamps; fixed-point raw
preservation; ASCII padding and non-allocating trailing-space trimming;
strong-identifier distinctions; flow/result helpers; event traits; and
feature-detection paths in both C++20 and C++23.

### Schema, pipeline, FFIR, and emitter

Each rule in [Schema format](schema-format.md) and
[Pipeline format](pipeline-format.md) needs a focused positive or negative
test. Coverage includes exact identifier acceptance/rejection, C++ keywords and
scope collisions, safe source emission, stable diagnostic codes and paths,
canonical FFIR JSON, and generated C++ compilation in C++20 and C++23.

Determinism tests compare semantic fingerprints, FFIR, and generated source
after changing comments, non-semantic TOML key/table order, working directory,
and repeated invocation. Two independent canonical-generation runs must have
the same SHA-256. Diagnostics and output are checked for absolute paths,
timestamps, host/user data, and other unstable provenance.

### BinaryFILE framing

Framing tests cover empty incomplete and empty complete sessions; one and
multiple frames; complete/incomplete termination; the zero marker after frames;
one-byte prefixes; truncated payloads; maximum 16-bit payload length; trailing
bytes under strict replay; offsets and ordinals; exact `consumed()` and
`remaining()` values; sticky terminal outcomes; and cooperative replay stop.

The cursor and strict generated replay are separate layers: the cursor exposes
bytes after an end marker, while strict replay classifies them as
`trailing_data_after_end_marker`.

### ITCH conformance

For every message in SPEC Section 15, independently reviewed coverage includes:

- at least one valid all-fields fixture;
- successful `itch50_all` decode with exact values;
- exact rejection at declared size minus one and plus one; and
- a valid skip under `itch50_order_events` when unselected.

The order-events pipeline also requires exact emission for
`A`, `F`, `E`, `C`, `X`, `D`, `U`, and `P`. Cross-message tests cover lowercase
`h` versus uppercase `H`, current message `O`, unknown discriminators under
both policies, empty payloads, complete/incomplete multi-frame input, malformed
known-unselected messages, sink stop, and exact replay counters.

### Projection and compile-fail

Compile-time and emitted-source tests prove that:

- an event has no unrequested member;
- generated code emits no load for an unrequested field;
- every selected event requires a valid sink overload;
- missing, ambiguous, throwing, and wrong-result overloads fail clearly;
- event layout is unchanged between C++20 and C++23 on the same ABI/config;
  and
- errors, skips, stops, and sink ordering do not depend on profile internals.

An instrumented `decoder_implementation` counts `load_unsigned` calls. Empty,
unknown, undersized, oversized, and malformed known-unselected payloads must
perform zero field loads. This directly proves that exact-size validation
precedes field access.

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
measures only the decode/replay call. Its fixed-storage sink does not allocate.
The test separately covers emitted, skipped, complete, incomplete, and
sink-stopped paths. This isolates FeedForge-owned work; arbitrary user sinks
remain responsible for their own behavior.

`feedforge_test_no_exceptions_rtti` is an explicit C++20 target that compiles and
runs the committed all-message decoder and replay adapter with
`-fno-exceptions -fno-rtti`. Hot entry points and sink calls are also checked as
`noexcept`.

The `rtsan` preset probes both the `[[clang::nonblocking]]` attribute and a
linkable `-fsanitize=realtime` runtime. Only a successful upstream-Clang probe
creates `hardening.rtsan_smoke`; unsupported compilers report that the optional
test is disabled and continue configuring. RTSan is not a minimum-toolchain
requirement, and its private compile/link options are never attached to exported
runtime or generated interface targets.

Test configuration also inspects exported runtime/canonical target properties
and fails if project warning or sanitizer compile/link options leak through an
interface.

## Sanitizers

The `sanitizers` preset is intended to build all applicable tests with current
Clang AddressSanitizer and UndefinedBehaviorSanitizer:

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
`raw_size` and derives isolated build-tree seeds from all 23 reviewed fixtures.
The reviewed TOML remains the source of truth. Committed error seeds cover empty
payload, unknown type, invalid size, complete/incomplete framing, truncated
prefix/payload, and trailing data. The generated manifest and aggregate replay
make the mapping auditable without host-dependent paths or parallel build
writes.

For identical bytes, the targets assert deterministic outcomes, no sink call
after an error, at most one sink call per payload, validation before counted
field loads, and coherent replay counters and terminal fields. ASan, UBSan, and
libFuzzer supply the crash, undefined-behavior, bounds, hang, and allocation
stress around those semantic assertions.

CI runs a short fixed-duration smoke over the seeds. Longer fuzz campaigns are
scheduled or manual, preserve useful reproducers, and report the toolchain,
target, corpus revision, duration, and failing input. The intended configure
path is:

```sh
cmake --preset fuzz
cmake --build --preset fuzz
```

The fuzz preset deliberately fails with an actionable diagnostic when the
selected Clang has no usable libFuzzer/ASan/UBSan runtime. Normal test builds
also compile the same three harnesses with fixed deterministic arbitrary inputs
as `hardening.arbitrary_input.*`; this is the local smoke path for AppleClang
installations that omit libFuzzer.

## Presets and CI evidence

For currently available focused tests:

```sh
cmake --preset fast
cmake --build --preset fast
ctest --preset fast
```

`dev` runs the complete configured suite plus examples; `release` repeats tests
with release-mode checks active. CI must cover minimum GCC/Clang C++20
runtime/generated builds with compiler disabled, current GCC/Clang full builds,
generated-code C++23 compatibility, Clang ASan+UBSan, no-exception/no-RTTI,
macOS arm64, Windows x64 Tier 2, installed generation, and fuzz smoke.

Warnings are errors for project and generated sources in CI. Hosted CI is
correctness infrastructure; it must not publish latency or throughput claims.
A release audit records exact commands, toolchain versions, outcomes, fixture
review state, and any permitted Tier 2 waiver. Green scaffold tests alone do
not satisfy the Definition of Done in SPEC Section 26.

Useful focused gates are:

```sh
cmake --preset compiler-off
cmake --build --preset compiler-off
ctest --preset compiler-off

cmake --preset no-exceptions-rtti
cmake --build --preset no-exceptions-rtti
ctest --preset no-exceptions-rtti

ctest --test-dir build/dev -L allocation --output-on-failure
```

Workflow names state Tier 1 versus Tier 2 policy. Tier 2 jobs are not silently
marked `continue-on-error`; a waiver requires the evidence listed in SPEC
Section 22. Workflow presence is not a claim that a hosted runner has passed.
