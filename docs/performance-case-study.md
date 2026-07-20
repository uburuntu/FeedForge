# FeedForge v0.1 performance case study

FeedForge generates checked C++20 decoders from a wire schema and a projection.
The v0.1 optimization question was deliberately comparative:

> Can the generated decoder do less work while preserving exact-size validation,
> decoded values, error and skip outcomes, sink order, deterministic generation,
> and the allocation-free runtime contract?

This was not a search for the smallest isolated timing. Candidate changes were
accepted only when a frozen benchmark contract showed a material target win,
bounded measurement noise, no meaningful regression elsewhere, and unchanged
correctness.

> **Claim boundary:** the results below compare specific content-addressed
> binaries on deterministic synthetic corpora, one Apple M4 Max host, and one
> AppleClang toolchain. They are not measurements of a live exchange feed,
> network or file I/O, production message-frequency distributions, tail
> latency, or a complete trading system. They do not establish production
> readiness or exchange certification.

## Frozen evaluation method

The procedure is defined by [benchmark contract 1.0.0](benchmarking.md). Its
workloads, correctness checks, metrics, and thresholds were fixed before the
retained changes were judged.

The public corpus contains one independently reviewed, hand-authored payload
for each of the 23 ITCH 5.0 message types. It provides deterministic type and
branch coverage, not a model of production traffic. Four workload shapes were
measured:

- all 23 types through the `itch50_all` pipeline;
- the eight messages selected by `itch50_order_events`;
- the 15 known messages skipped by that projection; and
- all 23 types through the projection, producing eight events and 15 skips.

Both generated `decode_one` and strict in-memory BinaryFILE replay were timed,
giving eight cases. Before any clock started, each process verified all 23
fixtures against both canonical decoders, exact emit/skip counters, strict
replay, and trailing-data rejection. Baseline and candidate recorded the same
correctness checksum.

File I/O, fixture parsing, hashing, allocation, host discovery, and correctness
validation were outside timed regions. The timed sink consumed each event and
updated a deterministic anti-elision checksum; its cost was intentionally part
of the measurement. Timed replay included construction performed by the public
replay function itself.

Each retained series used:

- seven independent processes per binary;
- five warm-up samples and 15 recorded samples per case and process;
- at least 50 ms per recorded sample after calibration;
- a batch floor of 256 workload rounds; and
- `std::chrono::steady_clock`.

A predeclared target required at least 5% lower median nanoseconds per message
and at least a 3% robust margin. The robust margin was the median improvement
minus `1.4826 * (baseline normalized MAD + candidate normalized MAD)`. Every
other case was guarded against a median regression greater than 2%, and each
series required cross-run MAD/median no greater than 3%. A passing public
comparison also required directional confirmation on a separate holdout.

## Measurement environment

The public comparison and corrected holdout used the same recorded host and
toolchain class.

| Property | Recorded value |
|---|---|
| Machine | `Mac16,6`, Apple M4 Max, arm64 |
| CPU inventory | 16 logical, 16 physical |
| Memory | 137,438,953,472 bytes (128 GiB) |
| OS | Darwin |
| Kernel | `25.5.0 Darwin Kernel Version 25.5.0: Mon Apr 27 20:41:15 PDT 2026; root:xnu-12377.121.6~2/RELEASE_ARM64_T6041` |
| Compiler | AppleClang `21.0.0.21000101`; `Apple LLVM 21.0.0 (clang-2100.1.1.101)` at `/usr/bin/c++` |
| Language and build | C++20, Release, `-O3 -DNDEBUG`, Ninja, interprocedural optimization off |
| Target flags | `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion`; no added ISA or `-march=native` flags |

macOS exposes no supported process-affinity API, so core placement remained
scheduler-controlled. The harness could not capture frequency, turbo state,
power mode, or thermal pressure. These limitations make the comparison useful
as evidence on this host, but the absolute nanosecond values must not be
generalized to another machine, operating system, compiler, or run.

## Public-corpus result

The final comparison used the original content-addressed baseline and the
binary containing all four retained refinements. Lower nanoseconds per message
is better. A target marker identifies the three cases named before their
respective changes were evaluated.

| Case | Baseline ns/message | Retained ns/message | Median improvement | Robust margin | Target |
|---|---:|---:|---:|---:|:---:|
| `decode_one/itch50_all/all_types` | 4.56358 | 3.52519 | 22.75% | 21.56% | yes |
| `replay_binary_file/itch50_all/all_types` | 4.20425 | 3.88289 | 7.64% | 6.63% | no |
| `decode_one/itch50_order_events/selected` | 5.08495 | 3.46971 | 31.77% | 31.03% | no |
| `replay_binary_file/itch50_order_events/selected` | 4.22943 | 3.87138 | 8.47% | 7.46% | no |
| `decode_one/itch50_order_events/unselected` | 3.39260 | 3.38611 | 0.19% | -0.51% | no |
| `replay_binary_file/itch50_order_events/unselected` | 2.31644 | 1.28160 | 44.67% | 43.08% | yes |
| `decode_one/itch50_order_events/mixed` | 3.64129 | 3.37412 | 7.34% | 6.65% | yes |
| `replay_binary_file/itch50_order_events/mixed` | 2.87314 | 1.66578 | 42.02% | 39.60% | no |

The serialized comparator reported correctness equivalence and
`optimization_win=true`. All three predeclared targets cleared both acceptance
thresholds. No guarded case regressed; the near-zero unselected `decode_one`
change was not claimed as a target win.

These medians describe repeated execution of the small frozen fixture
workloads. They do not imply billions of independent real messages per second,
nor do they include file access, network transport, packet recovery, queueing,
order-book work, or downstream application logic.

## The retained implementation

Four refinements make up the v0.1 retained source set:

1. **Inline the sink-specialized decode entry.** A compiler-guarded
   `FEEDFORGE_FORCE_INLINE` lets the caller see payload and sink state while
   retaining a portable plain-`inline` fallback.
2. **Fuse validation and dispatch only for all-selected pipelines.** When a
   projection selects every known message, generated code uses one switch with
   a case-local exact-size check followed by event construction. It removes a
   discriminator dispatch that is provably redundant for that pipeline shape.
3. **Dispatch selected messages first in partial pipelines.** Selected cases
   validate and construct directly; only misses enter the compact
   known-unselected size-and-skip switch.
4. **Use direct checked framing only for partial-pipeline replay.** Generated
   replay tracks a byte position and performs prefix, payload, end-marker, and
   trailing-data checks inline. All-selected replay keeps the general
   `binary_file_cursor` path.

The specialization decisions are made from resolved pipeline structure, not
runtime guesses. The retained code introduces no packed wire structs, typed
unaligned dereferences, architecture-specific flags, runtime polymorphism,
allocation, field read before exact-size validation, or benchmark-only path.
Observable events, values, outcomes, offsets, counters, and sink order remain
the same.

## Rejected hypotheses and lessons

Six correctness-preserving candidates were not retained. Positive values below
mean lower median nanoseconds per message on the named target.

| Hypothesis | Target median change | Robust margin | Decision |
|---|---:|---:|---|
| Fuse validation and dispatch for every partial-pipeline case | +0.03% | -1.04% | Below both target thresholds |
| Replace the general replay cursor for all pipelines | -1.12% | -2.19% | Target regressed |
| Put selected cases first to accelerate selected-message decode | -0.30% | -0.95% | Target regressed |
| Use exact-width `memcpy` plus endian normalization for field loads | +3.64% | +2.65% | Below 5% median and 3% robust thresholds |
| Directly aggregate-initialize generated events | +1.52% | +0.86% | Below both thresholds |
| Combine compact field loads and aggregate initialization | +3.66% | +3.05% | Robust threshold passed; 5% median threshold did not |

The main lesson was that source-level simplification is not a performance
result. Broadly fusing switches helped one structural pipeline shape but did
not help the selected-message path that motivated the generic version. A
general direct framing loop regressed its target, while restricting the idea
to partial pipelines later produced a retained result. Independently positive
field-materialization changes also failed to add up when combined.

The frozen thresholds prevented changing the success criterion after seeing a
plausible but insufficient result. Correctness was necessary for every
candidate, but correctness alone was not grounds to retain more generated-code
complexity.

## Code-size tradeoff

On this exact build, the original benchmark executable was 161,856 bytes and
the retained executable was 175,232 bytes: an increase of 13,376 bytes, or
8.26%. The increase appeared with the forced-inline decode entry; the later
retained refinements did not further change the executable size in this build.

The rejected compact-load candidates produced a 158,496-byte executable,
9.55% smaller than the retained binary and 2.08% smaller than the original.
That tradeoff was recorded rather than silently ignored, but code size was not
the predeclared target and could not substitute for a failed runtime threshold.
No claim is made that the same size deltas hold for another linker, compiler,
or platform.

## Redacted holdout conclusion

The authoritative holdout result is the corrected clean-baseline attestation
dated 2026-07-16. The corpus remains private at the case level, but its redacted
contract records 151 wholly synthetic messages and 4,877 aggregate bytes,
coverage of all official message types, field and padding variation, valid
BinaryFILE framing, and malformed cases outside timed streams. Its corpus ID is
`172de44f316733bdae45b7fe3f7a6873e378ae980424f016c36b6c50a9d834fb`.

Both sides passed normal conformance, independent schema verification,
generated-decoder checks, identity checks, and quality gates. The redacted
directional verdict was **confirm**: all three predeclared targets had positive
median improvement and positive uncertainty-adjusted margin, and the guarded
regression check passed.

The redacted equal-weight geometric means across frozen cases were 17.1769%
for `decode_one` and 27.0617% for replay. Their conservative maximum case noise
bounds were 2.5732% and 2.2250%, respectively. These aggregates confirm
direction on this second synthetic distribution; they are not production
throughput claims and do not disclose private case-level results.

The corrected baseline and candidate were contract-eligible seven-process
series captured in different windows of the same quiet session, not an
interleaved matched pair. Eligible series were chosen chronologically under
the frozen noise rule without using the verdict. Several other captures were
excluded by that rule; across the retained captures the worst cross-run
MAD/median was below 1.6%. The same macOS scheduling and telemetry limitations
apply to this evidence.

The corrected baseline's local evaluation commit was
`218809c5f487abda0823c709e461dc3ab17382ec`, with all four retained refinements
reverted. Its source and executable SHA-256 values were
`ff84d214d4eaeca81f832e2092f19ed2a36c85c4697d84eaf4702067682bb062`
and `87a47a66628105718d3811cf714961a9fd88f6a9c77423e9940a2d8258e3c88f`.
The candidate's local evaluation commit was
`d32dd76f09ca6b8a6e1ccd1b88690bc42e1b21d8`,
with source and executable SHA-256 values
`da09273935f3b943f79907afc6cdb0d2140aa46ee20e871d7263ac823a43eaeb`
and `a870d09aadcf703a929049ecc4945205f6a0d1b17284c7d3813e3b52498590e6`.

## Content-addressed evidence

The [v0.2.0 performance evidence bundle](https://github.com/uburuntu/FeedForge/releases/download/v0.2.0/feedforge-v0.2.0-performance-evidence.tar.gz)
publishes the baseline/candidate JSON and CSV, comparison, and corrected
redacted holdout attestation. Its manifest records both source-artifact and
published-file hashes; local filesystem paths are removed without changing any
measurement value. The evaluation commit IDs above are provenance identifiers,
not commits added to the public release history.

The following IDs bind the narrative to the retained source artifacts and
redacted holdout evidence.

| Artifact or semantic input | SHA-256 / content ID |
|---|---|
| Public corpus | `1737425a359d1759ec010dd56a2e12e920e34c028820c4301bad7d75fa839bd0` |
| Public correctness checksum | `88bdf0cd4c933375d258e5d0c7470142ccddc7ba392b2467c2bd0764effe72bc` |
| Schema semantic fingerprint | `5caf2a24f113157cc5e74069339801fd332582dea234e66dd34148a8f12b938a` |
| `itch50_all` pipeline fingerprint | `42f71275830db9a05233c775df3b25b889a5fb13fc72485fa7819201b6a9c5ca` |
| `itch50_order_events` pipeline fingerprint | `5091875ac081f047b55ccf8c8231b7aca268e4163d28e59956685944b1403ec1` |
| Public baseline source ID | `initial-local-baseline-v1` |
| Public baseline executable | `d3096d9d820552d6ca857d8ea08e5ab962db1f14598a744820ee539f6cb02a48` |
| Retained source ID | `accepted-iterations-01-05-08-09` |
| Retained executable | `15a8e107c0233b1370ce09ad282e2670f103226ecb59f89621e22124f1f639c9` |
| Retained source-set ID | `773f03815cc6730da92a7b50ccb521e326324ed27f5872504dc90f1b29cd10e2` |
| Ordered source/generated manifest | `375445abcab862c8f08639c7e273b268cfa975dff9c648704a5f821503434362` |
| Public baseline series JSON | `5903265f16bdb7bf57211157f519f093f889e27097dae60c30db703acbd862cb` |
| Public retained series JSON | `5970ed38b181a022840518a9ab1f485beb79e582a6cc1fdd08e9ba5e7876aa56` |
| Public comparison JSON | `ad7cf6d78589133026f187752a0472dea3008b444b78afc13cca753b1834b5bf` |
| Corrected redacted holdout attestation | `0f80176a7789fbd58f3d6435725c8ddfcce330503d1c9baa68d8004c899f5d6c` |
| Corrected holdout baseline series JSON | `bf172fd754669a6a0614d4fa3f0f64e58e34082e465587ad345e4a6670c402d6` |
| Corrected holdout candidate series JSON | `6acd640dc29ff7c4663ebf8ca43460f87853d8ca54ae16d714599a97a59b9444` |

The corrected redacted attestation supersedes the contaminated attestation
`22da217c86fccf4387b4c3bbf733299c353bb5ca3505437d3bdcee2d0122b799`.
That supersession matters: the final holdout conclusion is based on a control
that reverts all four retained refinements.

## Interpretation

The evidence supports a narrow conclusion: on the recorded Apple M4 Max and
AppleClang configuration, the four retained generated-code refinements reduced
median work on the predeclared synthetic targets without changing the checked
decoder contract, and the corrected synthetic holdout confirmed the same
direction.

It does not support a claim about exchange-feed latency, production traffic,
network stacks, file systems, end-to-end throughput, cross-platform speed, or
operational trading safety. FeedForge v0.1 remains an offline checked decoder,
not production trading infrastructure.
