# FeedForge v0.1.0 release notes

These notes describe the intended v0.1.0 scope. Do not publish a tag or release
while a blocker or pending gate remains in the
[FF-800 release audit](docs/release-audit.md).

## Scope

FeedForge v0.1.0 is an offline, ahead-of-time compiler and C++ runtime for a
deliberately narrow workflow:

- parse and validate versioned schema and projection TOML;
- lower it to canonical, serialisable FFIR;
- generate deterministic strict C++20 for the `portable_checked.v1` profile;
- decode the complete current 23-message Nasdaq TotalView-ITCH 5.0 inventory;
- replay in-memory Nasdaq BinaryFILE data through statically bound sinks; and
- install either the header-only runtime with canonical pipelines or the host
  compiler for custom build-tree generation.

The release includes the canonical `itch50_all` and `itch50_order_events`
pipelines, independently reviewed positive and wrong-size fixtures, source-tree
and installed CMake consumer examples, sanitizer targets, deterministic fuzz
corpus generation, three libFuzzer targets, and an optional self-documenting
Make command surface for source-tree development.

## Language, platform, and dependency policy

- Runtime and generated headers are strict C++20; `feedforgec` is C++23.
- The baseline runtime/generated minimums are GCC 11 and Clang 14. The host
  compiler requires GCC 13.2 or Clang 17 with the corresponding C++23 library,
  or newer.
- Linux x86-64 is Tier 1. Current AppleClang on macOS arm64 and MSVC 2022 on
  Windows x64 are Tier 2 under the policy in SPEC Section 22.
- Runtime and generated public targets have no third-party dependency.
  `toml++` is pinned and private to the optional host compiler.
- FeedForge-owned decode and replay work is `noexcept`, allocation-free after
  caller setup, and supported with exceptions and RTTI disabled.

Those entries state support policy, not completed hosted-CI results. The
required GitHub Actions matrix must pass on the release commit before release.

## Validation

The 2026-07-14 local FF-800 audit on macOS arm64 passed fresh Release,
compiler-disabled C++20, package/consumer, no-exception/no-RTTI, allocation,
ASan+UBSan, generation-determinism, corpus-determinism, and schema/source-lock
checks. Canonical generated headers matched two independent compiler runs
byte-for-byte.

The focused compiler-validation gaps identified by the audit are now covered
by split grammar, type/layout, identifier/diagnostic, and pipeline tests.
Runtime, compiler, FFIR/golden, and canonical generated provenance now agree
exactly on `0.1.0`; runtime API version `1` is unchanged. These two local
blockers are resolved.

This private, unpushed working tree is not evidence for GitHub Actions or a
literal clean clone. The local AppleClang installation also lacks a usable
libFuzzer runtime. Required Linux fuzz smoke, the complete hosted matrix, and a
clean-clone test of the final commit remain release gates; see the
[release audit](docs/release-audit.md) for exact commands and results.

## Limitations

FeedForge v0.1.0 is experimental, is not exchange-certified, and is not
production trading infrastructure. In particular:

- it provides no live networking, packet recovery, sequencing, order book,
  strategy, capture service, or database;
- it accepts in-memory BinaryFILE input and leaves file I/O or mapping to the
  caller;
- only `portable_checked.v1` is implemented; there is no runtime ISA or backend
  selection;
- unknown alpha/code values are preserved rather than semantically rejected;
- source compatibility is versioned, but no stable binary ABI is promised;
- no latency, throughput, or production-readiness claim is made; and
- exchange data is not bundled.

Authoritative protocol documents remain subject to their owners' terms. A
changed upstream checksum requires review rather than automatic schema or
fixture updates.
