# FeedForge

[![CI](https://github.com/uburuntu/FeedForge/actions/workflows/ci.yml/badge.svg)](https://github.com/uburuntu/FeedForge/actions/workflows/ci.yml)
[![Fuzz smoke](https://github.com/uburuntu/FeedForge/actions/workflows/fuzz-smoke.yml/badge.svg)](https://github.com/uburuntu/FeedForge/actions/workflows/fuzz-smoke.yml)
[![License: Apache-2.0](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)

FeedForge is an ahead-of-time compiler for checked market-data decode
pipelines. Its scope is deliberately narrow: compile a declarative
Nasdaq TotalView-ITCH 5.0 projection into strict C++20, then replay Nasdaq
BinaryFILE data through a statically bound sink.

FeedForge v0.1.0 is an **offline checked decoder**, not a feed handler or trading
platform. It is experimental, is not exchange-certified, and is not production
trading infrastructure.

## Scope

FeedForge v0.1 provides:

- a header-only C++20 runtime exposed as `FeedForge::runtime`;
- a C++23 host compiler exposed as `FeedForge::compiler`;
- deterministic generation of self-contained pipeline headers;
- portable, bounds-checked BinaryFILE and ITCH decoding; and
- allocation-free FeedForge-owned work on the per-message decode path.

| Surface | Contract |
|---|---|
| Runtime and generated code | Strict C++20; GCC 11 or Clang 14 minimum |
| Host compiler | C++23; GCC 13.2 or Clang 17 with a matching standard library |
| Platform policy | Linux x86-64 Tier 1; macOS arm64 and MSVC 2022 Tier 2 |
| Protocol scope | Nasdaq TotalView-ITCH 5.0 over in-memory BinaryFILE |
| Delivery | Statically bound typed sinks with explicit stop/error outcomes |

Live networking, packet recovery, order-book reconstruction, strategy logic,
and runtime schema parsing are outside the v0.1 boundary. See
[Architecture](docs/architecture.md) for component boundaries and deliberate
limits.

## Requirements

- CMake 3.25 or newer;
- Ninja for the shared project presets;
- a C++20 compiler for the runtime and generated code; and
- a C++23 compiler and standard library when building `feedforgec`.

The project has no external runtime dependency. On POSIX development hosts,
GNU Make is an optional source-tree command catalog; consumers and native
Windows developers continue to use CMake directly.

## Build and test

The default Make target lists the available development workflows. Start with
the environment check and focused suite:

```sh
make
make doctor
make quick
```

Use `make dev` for the full Debug suite and generated-byte check, `make release`
for an optimized build, `make sanitizers` for ASan+UBSan, and `make fuzz-smoke`
for bounded local libFuzzer runs. The wrapper delegates to the shared CMake
presets; direct CMake commands remain documented in
[the development workflow](docs/development.md).

To configure only the C++20 runtime without requiring a C++23 host toolchain:

```sh
cmake -S . -B build/runtime \
  -DFEEDFORGE_BUILD_COMPILER=OFF \
  -DFEEDFORGE_BUILD_TESTS=OFF \
  -DFEEDFORGE_BUILD_EXAMPLES=OFF
cmake --build build/runtime
```

## Generate and replay

The maintainer verification flow builds the host compiler, regenerates the two
canonical headers from their audited schema and pipelines, checks them
byte-for-byte, and builds the order-events replay example:

```sh
make generated-refresh CONFIRM=regenerate
make dev
make replay-empty
```

`generated-refresh` is explicitly guarded because it rewrites committed source;
ordinary builds and installs never do so. `replay-empty` uses
`itch50_order_events` against a valid empty complete session and reports
`status=complete` with two bytes consumed. Use `make replay REPLAY_FILE=<path>`
for a BinaryFILE you are permitted to process. The example reports exact replay
status, counters, consumed bytes, and framing or decode error category, and
performs file I/O and input allocation before entering replay.

To generate a one-off header under the build tree instead:

```sh
make pipeline-compile \
  GENERATED_OUTPUT=build/manual/itch50_order_events.hpp
```

## CMake consumers

Install FeedForge into a prefix:

```sh
make install PREFIX=build/install
```

Then consume the runtime and either committed canonical pipeline from a
separate CMake project:

```cmake
find_package(FeedForge CONFIG REQUIRED)
target_link_libraries(
  my_decoder
  PRIVATE
    FeedForge::generated::itch50_order_events
)
```

When the compiler is installed, `feedforge_generate()` creates a generated
interface target without writing into the source tree:

```cmake
feedforge_generate(
  NAME order_events
  SCHEMA nasdaq_totalview_itch_5_0
  PIPELINE "${CMAKE_CURRENT_SOURCE_DIR}/order_events.toml"
)

target_link_libraries(
  my_decoder
  PRIVATE FeedForge::generated::order_events
)
```

The committed targets are `FeedForge::generated::itch50_all` and
`FeedForge::generated::itch50_order_events`. They remain available in a
runtime-only install configured with `FEEDFORGE_BUILD_COMPILER=OFF`; this path
requires neither C++23 nor toml++. Custom generation requires an install that
contains `FeedForge::compiler`.

FeedForge v0.1 processes in-memory Nasdaq BinaryFILE data only. It does not
provide live networking, recovery, order-book reconstruction, exchange
certification, or production-trading guarantees.

## Documentation

- [Architecture](docs/architecture.md)
- [Development workflow](docs/development.md)
- [Adoption and integration guide](docs/adoption.md)
- [Schema format](docs/schema-format.md)
- [Pipeline format](docs/pipeline-format.md)
- [Generated C++ API](docs/generated-api.md)
- [Testing and fixture provenance](docs/testing.md)
- [Schema audit](docs/schema-audit.md)
- [v0.1.0 release notes](RELEASE_NOTES.md)

## Contributing and security

See [CONTRIBUTING.md](CONTRIBUTING.md) for development and review expectations.
Please report security-sensitive findings through the private process in
[SECURITY.md](SECURITY.md), not a public issue.

## License

FeedForge is licensed under the Apache License 2.0. See [LICENSE](LICENSE).
