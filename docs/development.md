# Development workflow

On POSIX development hosts, the root `Makefile` is the human-facing command
catalog for source-tree work. It is deliberately a thin wrapper:
`CMakePresets.json`, CMake targets, and CTest remain authoritative. Consumers
and native Windows developers use CMake directly, and the installed package
does not expose the developer wrapper.

Run `make` or `make help` to see the current command surface. Help is grouped by
workflow and automatically disables colour when output is redirected, when
`TERM=dumb`, or when `NO_COLOR` is set.

## First run

```sh
make doctor
make quick
make dev
```

`doctor` reports required tools, Python conformance-bundle readiness, and
optional LLVM/Docker capabilities. Python remains an optional development
capability, so a missing or older interpreter is reported without failing the
core environment check. `quick` runs the focused compiler and runtime suite.
`dev` runs the full Debug suite, installed consumers, examples, and the
byte-for-byte generated-header check.

The equivalent direct commands remain supported:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
cmake --build --preset dev --target check-generated
```

On native Windows with Visual Studio 2022 17.8 or newer, the equivalent full
MSVC validation is:

```powershell
cmake -S . -B build/msvc -G "Visual Studio 17 2022" -A x64 `
  -DFEEDFORGE_BUILD_COMPILER=ON `
  -DFEEDFORGE_BUILD_TESTS=ON `
  -DFEEDFORGE_BUILD_EXAMPLES=ON `
  -DFEEDFORGE_WARNINGS_AS_ERRORS=ON
cmake --build build/msvc --config Debug
ctest --test-dir build/msvc -C Debug --output-on-failure
cmake --build build/msvc --config Debug --target check-generated
```

This path builds `feedforgec` with native MSVC, runs the compiler and runtime
tests, and verifies that MSVC emits the committed canonical headers byte for
byte.

## Command groups

| Goal | Command |
|---|---|
| Focused development loop | `make quick` |
| Full Debug validation | `make dev` |
| Full Release validation | `make release` |
| ASan and UBSan | `make sanitizers` |
| Runtime-only strict C++20 | `make compiler-off` |
| Exceptions and RTTI disabled | `make no-exceptions-rtti` |
| All portable local gates | `make verify` |
| Extended LLVM/fuzz local matrix | `make verify-all` |
| Canonical generated-byte check | `make generated-check` |
| Synthetic conformance archives | `make conformance-bundle` |
| Upstream LLVM full suite | `make llvm-dev` |
| RealtimeSanitizer smoke | `make rtsan` |
| Seven bounded libFuzzer runs | `make fuzz-smoke` |
| Benchmark harness smoke | `make bench-smoke` |
| Deterministic release assets | `make release-assets-check` |
| Local install | `make install` |
| Runtime-only install | `make install-runtime` |
| Native-architecture Linux smoke | `make linux-smoke` |

`verify` and `verify-all` are local evidence only. The extended target requires
upstream LLVM with libFuzzer and RealtimeSanitizer support. Neither target
replaces the required hosted Linux compiler matrix, Windows job, or Linux
libFuzzer run.

The synthetic conformance bundle requires Python 3.11 or newer. CMake always
exposes the `conformance-bundle` target so it remains discoverable on machines
without a suitable interpreter; building that target then fails with setup
guidance while unrelated configure and build workflows remain available. The
Make wrapper passes the interpreter selected by `PYTHON` to CMake. Run
`make doctor` to check that interpreter before generation.

## Overrides

Make variables are ordinary command-line overrides:

```sh
make build PRESET=release JOBS=8
make test PRESET=dev CTEST_ARGS='-R compiler.validation_'
make install PREFIX="$PWD/out/feedforge"
make validate PIPELINE=pipelines/all_messages.toml
make pipeline-compile \
  PIPELINE=examples/consumer-template/custom_pipeline.toml \
  GENERATED_OUTPUT=build/manual/custom_events.hpp
```

Run `make variables` for the active high-value defaults. Lower-level overrides
are `CMAKE_ARGS`, `BUILD_ARGS`, and `CTEST_ARGS`. `CMAKE_ARGS` accepts cache and
configure options that preserve the selected preset's source and build tree;
`-B`, `-S`, and `--preset` are rejected to prevent split configure/build paths.

## Mutation guards

Commands that rewrite source or remove output trees require a literal token:

```sh
make generated-refresh CONFIRM=regenerate
make format-changed CONFIRM=format
make clobber CONFIRM=clobber
```

`generated-refresh` additionally refuses to overwrite already modified
canonical headers unless `FORCE=1` is supplied after review. It regenerates only
through `feedforgec`, runs `check-generated`, and never stages files. `clobber`
removes only the ignored `build/` and `out/` trees, refuses symlinked roots, and
will not run while the private holdout or benchmark result archive is present.
One-off generation commands reject traversal, symlink, and absolute paths that
resolve outside `build/`.

## Release assets

The standard-library release builder accepts only a full commit ID or an exact
tag named for the committed project version. It reads committed Git objects, so
working-tree changes and untracked files cannot enter an archive. Verify it with:

```sh
make release-assets-check \
  RELEASE_REVISION="$(git rev-parse HEAD)"
```

Build publishable assets into a new or empty ignored directory:

```sh
make release-assets \
  RELEASE_REVISION="$(git describe --exact-match --tags HEAD)" \
  RELEASE_OUTPUT_DIR=out/release/current
```

The command writes normalized `.tar.gz` and `.zip` source archives plus
`SHA256SUMS`. Archive metadata identifies the resolved commit. The checksum file
lists only the primary archives and intentionally does not hash itself.

## LLVM and fuzzing

Set `LLVM_CXX` to select an upstream compiler explicitly. On Homebrew systems,
the wrapper discovers keg-only LLVM automatically.

```sh
make llvm-dev
make llvm-sanitizers
make rtsan
make fuzz-smoke FUZZ_SECONDS=30
```

`make tidy` uses the same upstream LLVM compilation database and writes its
full advisory report to ignored `out/tidy/clang-tidy.log`, while the terminal
shows only the diagnostic counts.

Fuzzing always uses generated build-tree seed corpora. New corpus entries and
failure artifacts go under ignored `out/fuzz/`; reviewed fixture sources and
committed generated headers are never mutated. macOS defaults to
`ASAN_OPTIONS=detect_leaks=0` because the Darwin libFuzzer runtime retains its
RSS monitor thread at shutdown. Linux retains leak detection.

## Benchmark discipline

Smoke mode is always safe to run:

```sh
make bench-smoke
```

Full series collection requires an explicit label and immutable source ID, and
refuses a non-empty output directory:

```sh
make bench-run \
  BENCH_LABEL=candidate-01 \
  BENCH_SOURCE_ID="$(git rev-parse HEAD)"
```

Comparison likewise requires explicit series paths and refuses to infer target
IDs. Pass the predeclared targets, space-separated, in `BENCH_TARGETS`:

```sh
make bench-compare \
  BENCH_BASELINE=build/bench/results/baseline/series.json \
  BENCH_CANDIDATE=build/bench/results/candidate/series.json \
  BENCH_TARGETS='decode_one/itch50_all/all_types'
```

The frozen workload, acceptance thresholds, holdout policy, and publication
rules remain defined in [benchmarking.md](benchmarking.md).

## Editor integration

All shared presets export a compilation database. Point clangd at
`build/dev/compile_commands.json`, or create an ignored root symlink:

```sh
ln -s build/dev/compile_commands.json compile_commands.json
```

`.clang-format` and `.clang-tidy` intentionally remain at the repository root
because Clang tooling discovers them by walking parent directories. Other root
files are conventional project entry points; moving them would make the tree
look different without making it simpler to use.

`make linux-smoke` archives the Git-indexed working tree, including local edits,
into an ephemeral Docker volume. The test container mounts that source volume
read-only and places `build/` on tmpfs, so Docker Desktop and Colima do not need
the repository path in their host-sharing configuration. Untracked files are
intentionally excluded. When Docker reports daemon HTTP proxy settings, the
test container receives the same values for package installation.

The Docker platform follows the host architecture by default. Use an explicit
override when emulation is intentional, for example
`make linux-smoke DOCKER_PLATFORM=linux/amd64 LINUX_JOBS=2`; hosted Linux CI
remains the release gate for x86-64.
