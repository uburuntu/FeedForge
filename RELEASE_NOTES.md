# FeedForge v0.2.0

FeedForge v0.2.0 turns the offline checked decoder into a stronger integration
and evaluation surface while preserving its deliberately narrow product scope.
It remains experimental, is not exchange-certified, and is not production
trading infrastructure.

## Highlights

- Caller-buffered `chunked_replayer<Sink>` accepts arbitrary input boundaries
  through explicit `push()` and `finish()` calls. With sufficient scratch, its
  events, counters, offsets, stop behavior, and terminal result match one-shot
  replay.
- `make demo` builds and runs a non-empty, format-valid synthetic order-event
  session with stable field-level output and no captured exchange data.
- A separately transcribed, standard-library-only ITCH 5.0 oracle compares all
  projected fields and outcomes for all 23 message layouts. Its independence
  limit is documented explicitly.
- Four ASan+UBSan libFuzzer targets now cover framing, generated decode,
  independent differential decode, and one-shot/chunked replay equivalence.
  Push and pull-request runs remain bounded; a weekly campaign runs longer.
- Hosted portability evidence now includes full compiler/runtime generation on
  macOS arm64, MSVC runtime coverage plus ClangCL clean-output generation on
  Windows x64, and an emulated s390x big-endian runtime/generated-code probe.
- The specialization case study publishes the frozen method, all guarded
  results, rejected hypotheses, code-size cost, environment limits, and a
  redacted synthetic-holdout conclusion.
- Pinned GitHub Actions, CodeQL, secret scanning, private vulnerability
  reporting, and protected-main checks define the public repository boundary.

## Compatibility

The package version and generated-header identity are `0.2.0`. Runtime API
compatibility is now `2` because generated v0.2 headers use the new chunked
framing types. A v0.2 generated header must not be mixed with a v0.1 runtime;
the compile-time compatibility assertion rejects that combination.

The one-shot `binary_file_cursor`, `decoder`, and `replay_binary_file()` APIs
remain available. Schema, pipeline, and FFIR format versions remain `1`, and
`portable_checked.v1` remains the only implementation profile. Installed CMake
packages use same-minor compatibility while FeedForge is pre-1.0.

Runtime and generated headers remain strict C++20. Minimum tested compilers are
GCC 11 and Clang 14. The optional `feedforgec` host compiler remains C++23 and
requires GCC 13.2 or Clang 17 with a corresponding standard library, or newer.

## Validation

The release commit is required to pass:

- the hosted Linux compiler, minimum-toolchain, release, sanitizer,
  no-exception/no-RTTI, package-consumer, and generated-byte gates;
- release-blocking macOS, Windows, and emulated big-endian portability jobs;
- all four seeded libFuzzer jobs and the independent oracle self-test;
- deterministic clean-clone build, test, install, generation, demo, and replay;
- CodeQL security analysis plus reachable-history secret and artifact scans.

The annotated tag and GitHub Release identify the exact commit and hosted run
attempts. `feedforge-v0.2.0-performance-evidence.tar.gz` contains the retained
public JSON/CSV series and comparison plus the corrected redacted holdout
attestation used by the performance case study.

## Limitations

- FeedForge provides no live networking, packet recovery, sequencing, order
  book, strategy, capture service, database, or operational trading controls.
- Input is caller-owned in-memory data or caller-delivered chunks. FeedForge
  performs no file or network I/O.
- Chunked replay needs caller-owned scratch. A declared payload larger than the
  supplied span terminates with `framing_errc::insufficient_scratch`.
- QEMU proves big-endian code execution and ABI behavior, not physical s390x
  hardware performance or a new support tier.
- The Windows ClangCL compiler gate covers clean-output deterministic
  generation; the destination-replacement atomicity test remains outside that
  support claim.
- Performance evidence uses deterministic synthetic corpora on one recorded
  host and toolchain. It does not establish production latency, throughput, or
  cross-platform speed.
- Exchange data is not bundled. Authoritative protocol documents remain
  subject to their owners' terms.

See the [v0.1.0 GitHub Release](https://github.com/uburuntu/FeedForge/releases/tag/v0.1.0)
for the previous release notes and retained validation evidence.
