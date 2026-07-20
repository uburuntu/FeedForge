# FeedForge v0.1.0

FeedForge v0.1.0 is the first release of the offline, ahead-of-time compiler and
C++ runtime for checked Nasdaq TotalView-ITCH 5.0 projection pipelines.

## Highlights

- Versioned schema and projection TOML with strict validation and diagnostics.
- Deterministic FFIR lowering and self-contained generated C++20 headers.
- Complete current 23-message Nasdaq TotalView-ITCH 5.0 schema and reviewed
  positive/wrong-size fixtures.
- Canonical `itch50_all` and `itch50_order_events` generated pipelines.
- Bounds-checked in-memory Nasdaq BinaryFILE replay through statically bound
  typed sinks.
- Header-only runtime and generated public targets with no third-party runtime
  dependency.
- Runtime-only and compiler-enabled CMake packages, including downstream custom
  pipeline generation.
- Allocation-free FeedForge-owned decode/replay work after caller setup, with
  exceptions and RTTI disabled where required.

## Toolchains and platforms

- Runtime and generated headers are strict C++20. Minimum tested compilers are
  GCC 11 and Clang 14.
- The optional `feedforgec` host compiler is C++23 and requires GCC 13.2 or
  Clang 17 with a corresponding standard library, or newer.
- Linux x86-64 is Tier 1. AppleClang on macOS arm64 and MSVC 2022 on Windows x64
  are Tier 2.
- `toml++` is pinned and private to the optional host compiler.

## Validation

Publication requires the release commit to pass the full hosted
compiler/platform matrix, Linux ASan+UBSan with leak detection, all three
bounded libFuzzer jobs, deterministic generation checks, installed
runtime/compiler consumers, no-exception/no-RTTI coverage, and a clean-clone
build/install/replay audit. The published annotated tag records the exact
commit, and the GitHub Release links the exact hosted run attempts retained as
release evidence.

The audited schema and fixture inventory is documented in
[docs/schema-audit.md](docs/schema-audit.md). Benchmark infrastructure is
available for reproducible engineering work, but v0.1.0 makes no latency,
throughput, or production-readiness claim.

## Limitations

FeedForge v0.1.0 is experimental, is not exchange-certified, and is not
production trading infrastructure. In particular:

- it provides no live networking, packet recovery, sequencing, order book,
  strategy, capture service, or database;
- it accepts caller-owned in-memory BinaryFILE input and leaves file I/O or
  mapping to the caller;
- only `portable_checked.v1` is implemented; there is no runtime ISA or backend
  selection;
- unknown alpha/code values are preserved rather than semantically rejected;
- source compatibility is versioned, but no stable binary ABI is promised;
- no latency, throughput, or production-readiness claim is made; and
- exchange data is not bundled.

Authoritative protocol documents remain subject to their owners' terms. A
changed upstream checksum requires review rather than automatic schema or
fixture updates.
