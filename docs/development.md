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

On native Windows with the Visual Studio 2022 toolset used by the Windows 2022
CI image, the equivalent full MSVC validation is:

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
byte. CMake rejects native MSVC older than 19.38 (Visual Studio 2022 17.8), but
the release matrix qualifies the toolset supplied by the Windows 2022 image
rather than an exact 19.38 installation.

## Command groups

| Goal | Command |
|---|---|
| Focused development loop | `make quick` |
| Full Debug validation | `make dev` |
| Full Release validation | `make release` |
| ASan and UBSan | `make sanitizers` |
| Exception-enabled toml++ compiler ABI | `make compiler-exceptions` |
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
| Pre-timing correctness gate | `make bench-correctness` |
| Qualified benchmark series | `make bench-run BENCH_LABEL=... BENCH_SOURCE_ID=...` |
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
Benchmark observations use a separate `BENCHMARK_SHA256SUMS`; never add
host-specific timing evidence to the deterministic source checksum file.

## 1.0 release qualification

A 1.0 release candidate is qualified from one clean, exact commit. Record the
commit, toolchain identities, commands, and results; do not combine results from
different candidates. The portable and extended local matrices begin with:

```sh
make verify-all
make test-arbitrary-input
make conformance-bundle
make test-installed
actionlint
```

The normal Debug, Release, sanitizer, and upstream-LLVM configurations use the
default non-throwing toml++ ABI and run the complete compiler suite. The
`compiler-exceptions` preset separately sets `FEEDFORGE_TOML_EXCEPTIONS=ON`,
runs the full compiler/runtime suite, and compares canonical generated bytes.
Both compiler configurations are required. The `no-exceptions-rtti` gate is a
different constraint: it applies only to generated/runtime C++ built with
language exceptions and RTTI disabled, and does not replace either compiler
configuration. All seven standalone arbitrary-input tests and all seven
ASan+UBSan libFuzzer targets must pass; fuzz evidence records the exact target,
source, toolchain, corpus, duration, and artifact outcome.

Qualification also exercises both runtime and compiler consumers through the
pinned real vcpkg overlay, regenerates and compares canonical headers, runs the
Python conformance and benchmark mutation tests, and reproduces deterministic
release archives. Extract each candidate archive into a clean directory, then
configure, build, test, install, and run an external CMake consumer from that
archive. This catches Git-worktree assumptions that an in-place build cannot.

After the local candidate is fixed, push that exact commit and require the
hosted `C++ security analysis`, `CI required`, and `Fuzz required` checks for the
same SHA. A tag, release, local result from another commit, or green individual
job is not a substitute for those exact aggregate checks. Build final source
assets from the exact tag, verify `SHA256SUMS`, download the published assets,
and compare them byte-for-byte with the local copies.

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

Smoke mode is deterministic correctness validation. Hosted CI runs it but does
not publish its timings:

```sh
make bench-smoke
```

`bench-correctness` runs the full dev, conformance, generated-byte, and
benchmark smoke gates. Full series collection requires an explicit label and
the exact clean 40-character `HEAD`, refuses CI and non-empty output or evidence
directories, and waits 120 seconds after correctness before timing:

```sh
make bench-run \
  BENCH_LABEL=v1.0.0-qualified \
  BENCH_SOURCE_ID="$(git rev-parse HEAD)" \
  BENCH_COOLDOWN_SECONDS=120
```

On macOS the target requires AC power and Automatic/legacy-off power mode 0 at
both endpoints. Preserve the pre/post `pmset` power and thermal records in
`BENCH_EVIDENCE_DIR`, then revalidate the aggregate against all seven raw runs:

```sh
python3 benchmarks/benchmark.py validate-series \
  --series build/bench/results/v1.0.0-qualified/series.json \
  --runs-dir build/bench/results/v1.0.0-qualified
```

Keep raw JSON/CSV and the capture-sanitized, hash-bound text files unchanged so
portable validation can rebuild the series. The mechanical log scrubber is not
exhaustive; manually review every asset. Use `benchmark.py redact-log` for the
separately captured full-verification log. Do not upload private holdout
material, recursive build directories, credentials, proprietary data, or
licensed captures.

Comparison likewise requires explicit series paths and refuses to infer target
IDs. Pass the predeclared targets, space-separated, in `BENCH_TARGETS`:

```sh
make bench-compare \
  BENCH_BASELINE=build/bench/results/baseline/series.json \
  BENCH_CANDIDATE=build/bench/results/candidate/series.json \
  BENCH_TARGETS='decode_one/itch50_all/all_types'
```

The frozen workload, acceptance thresholds, evidence files, privacy checks, and
claim boundary remain defined in [benchmarking.md](benchmarking.md). The
retained v0.6 series is a baseline, not an optimization or
production-throughput claim.

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
