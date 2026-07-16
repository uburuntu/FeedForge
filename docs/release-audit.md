# FF-800 v0.1.0 release audit

- Audit date: 2026-07-14
- Repository: `uburuntu/FeedForge` (private)
- Branch: `main`
- Audited HEAD: `e2db943aed8ab37e82b64d800b5676f91033b632`
- Initial audit workspace: `/tmp/feedforge-ff800-20260714-1610`
- Focused blocker re-audit workspace:
  `/tmp/feedforge-ff800-reaudit-20260714-1643`

## Recommendation

**LOCAL GO: both prior local blockers are resolved.**

**RELEASE PENDING: do not tag or publish v0.1.0 until the remaining hosted,
Linux libFuzzer, and post-commit gates pass.**

The two local blockers identified by the initial FF-800 audit are resolved:

1. **Resolved locally — focused compiler validation.** Six focused test
   executables now cover the individual acceptance/rejection rules in SPEC
   §§10.4, 14.2–14.3, and 16.2–16.3 with exact diagnostic codes, useful text,
   object paths, and normalized source paths.
2. **Resolved locally — release identity.** CMake/package metadata, the public
   runtime constant, `feedforgec --version`, FFIR/golden provenance, and both
   canonical generated headers now report exactly `0.1.0`.

The remaining release gates are:

1. **Pending hosted CI.** The implementation is private, uncommitted, and
   unpushed. There is no GitHub Actions result for the required Linux
   GCC/Clang minimum/current matrix, Windows Tier 2, or installed generation.
2. **Pending Linux libFuzzer smoke.** Local AppleClang has no usable
   libFuzzer runtime, and the required hosted Linux fuzz workflow is unrun.
3. **Pending clean-clone-after-commit.** A literal clone cannot contain the
   uncommitted implementation. The final release commit must be cloned into a
   new directory and tested with the documented presets before release.

This resolution did not change audited Nasdaq schema, pipeline, or fixture
semantics, weaken validation, or hand-edit generated headers. No parser fix was
needed, no SPEC contradiction was found, and no commit or tag was created.

## Evidence boundary

The initial audit inspected the full repository, both staged and unstaged
tracked diffs, and the untracked implementation reported at that time. This
focused re-audit inspected only the actual changes and evidence surfaces for
the two prior local blockers: compiler-validation tests and registration,
runtime/compiler/package identity, canonical generated provenance, and runtime
API compatibility. It does not re-certify unrelated subsequent working-tree
changes. A passing local target is not treated as hosted-CI or clean-clone
evidence. Workflow presence is not treated as a run.

The Definition of Done mapping is in
[requirements-traceability.md](requirements-traceability.md). Its current
summary is 18 **Pass**, one **Pending clean-clone-after-commit**, and two
**Pending external CI** items. The two separate local failures are now closed;
the local recommendation is **GO**. The release remains **Pending**, not
passed, while hosted CI, Linux libFuzzer, and post-commit clean-clone evidence
is unavailable.

## Local blocker resolution evidence

### Focused validation coverage

The compiler tests are split by rule family so one broad test cannot obscure a
failure:

- `compiler.validation_schema_grammar`: every required top-level/type/message/
  field key; unknown keys at top level, `types`, `messages`, and
  `messages.fields`; TOML value/container types; duplicate keys; signed-64-bit
  integer bounds; optional types, documentation, defaults, and accepted TOML
  integer syntax.
- `compiler.validation_schema_types`: every supported built-in width and
  logical mapping; default logical types; decimal scale `0`/`18`, missing,
  negative, `19`, and forbidden scale; unsupported physical/logical kinds;
  width bounds; each physical/logical incompatibility; undeclared and
  fixed-width field types; allowed-value width.
- `compiler.validation_schema_layout`: message sizes `1`, `65535`, and rejected
  `0`, `-1`, `65536`; printable discriminator endpoints and invalid byte forms;
  duplicate type/message/field names and discriminators; endian/discriminator
  layout; offset/width representation bounds; overlap, gap, beyond-end, and
  incomplete-final-coverage branches; explicit reserved coverage; every
  discriminator role/value presence, absence, multiplicity, type, layout, and
  mismatch combination.
- `compiler.validation_identifiers_diagnostics`: accepted/rejected lexical
  forms in every schema/type/message/field/pipeline/event/profile scope; all 92
  compiler-recognized C++ keywords; leading-underscore, double-underscore,
  punctuation, digit, uppercase, and non-ASCII forms; all seven reserved
  built-in type names; namespace component/separator forms; exact spelling
  preservation; syntax, structural, semantic, I/O, object, line/column, text,
  hint, and non-canonicalized normalized-path diagnostics.
- `compiler.validation_pipeline_grammar`: every required top-level and emit
  key; unknown top-level and nested `emit` keys; wrong TOML types and non-table
  emits; duplicate keys; reordered/commented positive input.
- `compiler.validation_pipeline_semantics`: schema/profile/policy mismatch;
  no emits; source width, unknown/duplicate/case-sensitive sources; duplicate
  events; empty, duplicate, unknown, wildcard/mixed, discriminator, reserved,
  and unrepresentable fields; wildcard expansion/exclusion/wire order; explicit
  field order; generated-scope collisions; canonical emit order.

Every invalid case checks its stable `FF*` code and category-specific useful
text; parser/model cases also check the relevant object path, and all semantic
helpers check the normalized supplied source path. No production parser or
validator change was required.

The focused re-audit used a new build directory and ran the complete compiler
test family plus direct runtime/generated identity assertions:

```sh
mkdir "/tmp/feedforge-ff800-reaudit-20260714-1643"
cmake -S . \
  -B "/tmp/feedforge-ff800-reaudit-20260714-1643/focused" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DFEEDFORGE_BUILD_COMPILER=ON \
  -DFEEDFORGE_BUILD_TESTS=ON \
  -DFEEDFORGE_BUILD_EXAMPLES=OFF \
  -DFEEDFORGE_BUILD_FUZZERS=OFF \
  -DFEEDFORGE_WARNINGS_AS_ERRORS=ON
cmake --build \
  "/tmp/feedforge-ff800-reaudit-20260714-1643/focused" \
  --target \
    feedforgec \
    feedforge_test_compiler_frontend \
    feedforge_test_compiler_validation_identifiers_diagnostics \
    feedforge_test_compiler_validation_pipeline_grammar \
    feedforge_test_compiler_validation_pipeline_semantics \
    feedforge_test_compiler_validation_schema_grammar \
    feedforge_test_compiler_validation_schema_layout \
    feedforge_test_compiler_validation_schema_types \
    feedforge_test_unit_runtime_primitives \
    feedforge_test_canonical_headers_cpp20 \
    feedforge_test_canonical_headers_cpp23 \
  -j 4
ctest \
  --test-dir "/tmp/feedforge-ff800-reaudit-20260714-1643/focused" \
  --output-on-failure \
  -R \
  '^(compiler\.(version|frontend|validation_.*)|unit\.runtime_primitives|integration\.canonical_headers_cpp(20|23))$'
cmake --build \
  "/tmp/feedforge-ff800-reaudit-20260714-1643/focused" \
  --target check-generated
cmake --build \
  "/tmp/feedforge-ff800-reaudit-20260714-1643/focused" \
  --target \
    feedforge_test_generated_cpp20 \
    feedforge_test_generated_cpp23 \
    feedforge_test_compiler_emitter \
  -j 4
ctest \
  --test-dir "/tmp/feedforge-ff800-reaudit-20260714-1643/focused" \
  --output-on-failure \
  -R '^compiler\.'
```

Result: configure and all requested builds exited 0. The identity-focused run
passed **11/11** tests in 2.30 seconds; the complete SPEC §21.3 compiler family
passed **15/15** in 1.09 seconds. `check-generated` reported `Checking committed
generated headers byte-for-byte` and exited 0.

The targeted-fix verification had also run the documented presets:

```sh
cmake --preset fast
cmake --build --preset fast -j 4
ctest --preset fast
ctest --test-dir build/fast --output-on-failure \
  -R '^compiler\.validation_'

cmake --preset dev
cmake --build --preset dev -j 4
ctest --preset dev
cmake --build --preset dev --target check-generated

cmake --preset release
cmake --build --preset release -j 4
ctest --preset release
cmake --build --preset release --target check-generated

cmake --preset compiler-off
cmake --build --preset compiler-off -j 4
ctest --preset compiler-off
```

Results: focused validation **6/6**, Fast compiler/unit **17/17**, Dev
**31/31**, Release **31/31**, and compiler-off strict C++20 **12/12** passed.
Both Dev and Release `check-generated` targets passed byte-for-byte.
The Dev/Release suites include strict warning-as-error, no-exception/no-RTTI
generated C++20 and C++23 synthetic tests, canonical-header C++20/C++23 tests,
layout comparison, and `hardening.no_exceptions_rtti`.

### v0.1.0 identity and regenerated artifacts

`CMakeLists.txt`, `feedforge::version_string`, exact `feedforgec --version`
output, canonical/golden generated preambles, generated
`pipeline_metadata::generator_version`, and FFIR provenance now agree on
`0.1.0`. The exact CLI result is:

```text
feedforgec 0.1.0
```

The focused re-audit regenerated both canonical headers independently and
compared them with the committed artifacts:

```sh
mkdir "/tmp/feedforge-ff800-reaudit-20260714-1643/generated"
"/tmp/feedforge-ff800-reaudit-20260714-1643/focused/src/feedforgec/feedforgec" \
  --version
"/tmp/feedforge-ff800-reaudit-20260714-1643/focused/src/feedforgec/feedforgec" \
  compile \
  --schema schemas/nasdaq/totalview_itch_5_0.toml \
  --pipeline pipelines/all_messages.toml \
  --output \
    "/tmp/feedforge-ff800-reaudit-20260714-1643/generated/itch50_all.hpp"
"/tmp/feedforge-ff800-reaudit-20260714-1643/focused/src/feedforgec/feedforgec" \
  compile \
  --schema schemas/nasdaq/totalview_itch_5_0.toml \
  --pipeline pipelines/order_events.toml \
  --output \
    "/tmp/feedforge-ff800-reaudit-20260714-1643/generated/itch50_order_events.hpp"
shasum -a 256 \
  generated/include/feedforge/generated/nasdaq/itch50_all.hpp \
  "/tmp/feedforge-ff800-reaudit-20260714-1643/generated/itch50_all.hpp" \
  generated/include/feedforge/generated/nasdaq/itch50_order_events.hpp \
  "/tmp/feedforge-ff800-reaudit-20260714-1643/generated/itch50_order_events.hpp"
cmp -s \
  generated/include/feedforge/generated/nasdaq/itch50_all.hpp \
  "/tmp/feedforge-ff800-reaudit-20260714-1643/generated/itch50_all.hpp"
cmp -s \
  generated/include/feedforge/generated/nasdaq/itch50_order_events.hpp \
  "/tmp/feedforge-ff800-reaudit-20260714-1643/generated/itch50_order_events.hpp"
```

Both `cmp` checks exited 0. The current canonical hashes are
`e1794d9005d52f9c59258d4b1e65240eb925f41a8df8e048fff97af3f1e02fe9`
and
`99c5cf11618ec7a7016a2a8adbf5ea059eb5d48ff0052343d5018f27b49779cd`.

`feedforge::runtime_api_version` remains `1`; runtime, canonical generated
C++20, and canonical generated C++23 compile-time assertions passed. Canonical
headers were written only by the `regenerate` target. The synthetic golden
header and FFIR golden were written by `feedforgec` from their compiler
fixtures.

Two independent canonical generations used relative inputs from the repository
and absolute inputs from a different working directory. Each run matched the
committed header byte-for-byte, and the two FFIR dumps matched each other:

| Artifact | SHA-256 |
|---|---|
| `itch50_all.hpp` — committed, run A, run B | `e1794d9005d52f9c59258d4b1e65240eb925f41a8df8e048fff97af3f1e02fe9` |
| `itch50_order_events.hpp` — committed, run A, run B | `99c5cf11618ec7a7016a2a8adbf5ea059eb5d48ff0052343d5018f27b49779cd` |
| `itch50_all.ffir.json` — run A and run B | `e7f7ac90c9ece9d74bf8861caf88518cfc8399501791206ccd844a52ab852cf8` |
| `itch50_order_events.ffir.json` — run A and run B | `c3d6181b7c8fac5f436ac3556d8435c8a2905d94e2f3e2083a2b5bfb20aa56cb` |
| `tests/golden/synthetic_pipeline.hpp` | `fa5dede39970fe80b34dbc41774a8425730f7f8082b1bf47fe05a5ee3c0869f6` |
| `tests/fixtures/compiler/valid.ffir.json` | `10d5eb85e48f64eca1f97469a27a2d104c66e837ee912391e196efce6aa612e3` |

## Audit environment

- macOS 26.5.1, build `25F80`; Darwin `25.5.0`; arm64
- Apple clang `21.0.0 (clang-2100.1.1.101)`, target
  `arm64-apple-darwin25.5.0`
- CMake `4.4.0`
- Ninja `1.13.2`
- Python `3.14.3`
- Git branch `main`, HEAD
  `e2db943aed8ab37e82b64d800b5676f91033b632`

Captured with:

```sh
uname -srm
sw_vers
cmake --version
ninja --version
/usr/bin/c++ --version
clang++ --version
python3 --version
git rev-parse HEAD
git branch --show-current
git remote -v
git status --short
```

## Initial blocker evidence (superseded)

This section preserves the evidence that identified the two local blockers.
The resolution evidence and current status are recorded above.

### Initial focused-validation finding

The following repository searches were run after the complete test inventory
was inspected:

```sh
rg -n '65535|65536' tests/compiler
rg -n \
  'messages\[0\]\.size|scale|fields\[0\]\.role\.reset|fields\[0\]\.value\.reset|unknown_top_level' \
  tests/compiler/frontend.cpp
rg -n \
  'unknown_[A-Za-z_]+\s*=|unknown_(type|message|field|emit)|missing.*(type|message|field|emit)' \
  tests/compiler
```

The first command found no compiler test. The second found only size `0`,
decimal scale `19`, scale on a non-decimal type, both discriminator role and
value removed together, and top-level unknown-key mutations. The third found
no nested type/message/field/emit unknown- or missing-key fixture. Direct
inspection of `model.cpp` confirms separate branches for size above `65535`,
missing decimal scale, and a present discriminator role with no value. This is
the exact evidence for the SPEC §21.3 failure; passing the existing
`compiler.frontend` test does not exercise those branches.

### Initial release-identity finding

```sh
"/tmp/feedforge-ff800-20260714-1610/release/src/feedforgec/feedforgec" --version
```

The initial result used a development-suffixed identity while
`CMakeLists.txt` and the installed `FeedForgeConfigVersion.cmake` declared
`0.1.0`. The discrepancy was present in built, installed, public, and generated
release identity surfaces before the resolution recorded above.

## Initial pre-resolution build and test results

Every build below used a newly created directory under
`/tmp/feedforge-ff800-20260714-1610`; no prior build result was used as release
evidence. The audit root was created once with:

```sh
mkdir "/tmp/feedforge-ff800-20260714-1610"
```

### Release, complete local suite, and check-generated

```sh
cmake -S . \
  -B "/tmp/feedforge-ff800-20260714-1610/release" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DFEEDFORGE_BUILD_COMPILER=ON \
  -DFEEDFORGE_BUILD_TESTS=ON \
  -DFEEDFORGE_BUILD_EXAMPLES=ON \
  -DFEEDFORGE_BUILD_FUZZERS=OFF \
  -DFEEDFORGE_WARNINGS_AS_ERRORS=ON
cmake --build "/tmp/feedforge-ff800-20260714-1610/release" -j 4
ctest --test-dir "/tmp/feedforge-ff800-20260714-1610/release" \
  --output-on-failure
cmake --build "/tmp/feedforge-ff800-20260714-1610/release" \
  --target check-generated
```

Result: all commands exited 0. AppleClang compiled 49 build actions. CTest
passed **25/25** tests in 8.18 seconds. This includes CLI/atomicity,
source-tree generation, installed canonical and custom-generation consumers,
C++20/C++23 generated semantics/layout, all 23 ITCH fixtures, BinaryFILE,
allocation, status branches, standalone arbitrary-input harnesses,
no-exception/no-RTTI, and canonical header compilation. `check-generated`
reported `Checking committed generated headers byte-for-byte` and exited 0.

### Compiler-disabled strict C++20 install and consumer

```sh
cmake -S . \
  -B "/tmp/feedforge-ff800-20260714-1610/compiler-off" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_STANDARD=20 \
  -DCMAKE_CXX_EXTENSIONS=OFF \
  -DFEEDFORGE_BUILD_COMPILER=OFF \
  -DFEEDFORGE_BUILD_TESTS=ON \
  -DFEEDFORGE_BUILD_EXAMPLES=ON \
  -DFEEDFORGE_BUILD_FUZZERS=OFF \
  -DFEEDFORGE_WARNINGS_AS_ERRORS=ON
cmake --build "/tmp/feedforge-ff800-20260714-1610/compiler-off" -j 4
ctest --test-dir "/tmp/feedforge-ff800-20260714-1610/compiler-off" \
  --output-on-failure
cmake --install "/tmp/feedforge-ff800-20260714-1610/compiler-off" \
  --prefix "/tmp/feedforge-ff800-20260714-1610/runtime-install"
cmake -S tests/consumer \
  -B "/tmp/feedforge-ff800-20260714-1610/runtime-consumer" \
  -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_STANDARD=20 \
  -DCMAKE_CXX_EXTENSIONS=OFF \
  -DCMAKE_PREFIX_PATH="/tmp/feedforge-ff800-20260714-1610/runtime-install" \
  -DFEEDFORGE_CONSUMER_KIND=canonical
cmake --build "/tmp/feedforge-ff800-20260714-1610/runtime-consumer" \
  --target run-feedforge-consumer -j 4
```

Result: all commands exited 0. CTest passed **12/12** in 5.64 seconds. The
install contained runtime headers, both canonical generated headers, schemas,
pipelines, package config/version/generation modules, and no compiler
executable. The independently configured strict-C++20 canonical consumer built
and ran.

The installed target file was also inspected. `FeedForge::runtime` exports only
`cxx_std_20` and include directories. Each canonical target exports C++20,
include directories, and a link to `FeedForge::runtime`; there is no
third-party link library or leaked warning/sanitizer/exception/optimization
option.

### AddressSanitizer and UndefinedBehaviorSanitizer

```sh
cmake -S . \
  -B "/tmp/feedforge-ff800-20260714-1610/sanitizers" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DFEEDFORGE_BUILD_COMPILER=ON \
  -DFEEDFORGE_BUILD_TESTS=ON \
  -DFEEDFORGE_BUILD_EXAMPLES=ON \
  -DFEEDFORGE_BUILD_FUZZERS=OFF \
  -DFEEDFORGE_WARNINGS_AS_ERRORS=ON \
  -DFEEDFORGE_ENABLE_ASAN=ON \
  -DFEEDFORGE_ENABLE_UBSAN=ON
cmake --build "/tmp/feedforge-ff800-20260714-1610/sanitizers" -j 4
ctest --test-dir "/tmp/feedforge-ff800-20260714-1610/sanitizers" \
  --output-on-failure
```

Result: all commands exited 0; **25/25** tests passed in 9.78 seconds with
ASan+UBSan instrumentation and no sanitizer finding.

### Exceptions, RTTI, allocation, and arbitrary input

The fresh Release, compiler-off, and sanitizer suites all ran:

- `hardening.no_exceptions_rtti`: passed; its target compiles C++20 generated
  runtime/replay code with `-fno-exceptions -fno-rtti`;
- `hardening.allocation`: passed for emitted, skipped, complete, incomplete,
  and stopped paths after setup; and
- all three `hardening.arbitrary_input.*` standalone harnesses: passed,
  including under ASan+UBSan.

These are direct results from the fresh CTest runs above, not stale focused
invocations.

### libFuzzer and optional RTSan availability

```sh
cmake -S . \
  -B "/tmp/feedforge-ff800-20260714-1610/fuzz" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DFEEDFORGE_BUILD_COMPILER=OFF \
  -DFEEDFORGE_BUILD_TESTS=OFF \
  -DFEEDFORGE_BUILD_EXAMPLES=OFF \
  -DFEEDFORGE_BUILD_FUZZERS=ON \
  -DFEEDFORGE_WARNINGS_AS_ERRORS=ON
```

Result: configure exited 1 with the intended diagnostic:
`The selected Clang does not provide a usable libFuzzer/ASan/UBSan runtime`.
No actual fuzz run is claimed. SPEC requires Linux fuzz smoke, so this is
**Pending external CI**, not a pass and not a FeedForge runtime failure.

```sh
cmake -S . \
  -B "/tmp/feedforge-ff800-20260714-1610/rtsan" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DFEEDFORGE_BUILD_COMPILER=OFF \
  -DFEEDFORGE_BUILD_TESTS=ON \
  -DFEEDFORGE_BUILD_EXAMPLES=OFF \
  -DFEEDFORGE_BUILD_FUZZERS=OFF \
  -DFEEDFORGE_WARNINGS_AS_ERRORS=ON \
  -DFEEDFORGE_ENABLE_RTSAN=ON
```

Result: configure exited 0 and reported:
`RealtimeSanitizer smoke disabled: the selected compiler is not upstream
Clang`. RTSan is optional under SPEC §7.3.

## Initial pre-resolution generated-artifact evidence

The Release `check-generated` target passed. Independently, the fresh
`feedforgec` generated each canonical pipeline twice: once from repository-
relative inputs and once from a different working directory with absolute input
paths. Both runs emitted FFIR. `shasum -a 256` and `cmp -s` compared the two
runs and the committed headers.

```sh
mkdir -p \
  "/tmp/feedforge-ff800-20260714-1610/determinism/run-a" \
  "/tmp/feedforge-ff800-20260714-1610/determinism/run-b/cwd"
"/tmp/feedforge-ff800-20260714-1610/release/src/feedforgec/feedforgec" \
  compile \
  --schema schemas/nasdaq/totalview_itch_5_0.toml \
  --pipeline pipelines/all_messages.toml \
  --output "/tmp/feedforge-ff800-20260714-1610/determinism/run-a/itch50_all.hpp" \
  --dump-ir "/tmp/feedforge-ff800-20260714-1610/determinism/run-a/itch50_all.ffir.json"
"/tmp/feedforge-ff800-20260714-1610/release/src/feedforgec/feedforgec" \
  compile \
  --schema schemas/nasdaq/totalview_itch_5_0.toml \
  --pipeline pipelines/order_events.toml \
  --output "/tmp/feedforge-ff800-20260714-1610/determinism/run-a/itch50_order_events.hpp" \
  --dump-ir "/tmp/feedforge-ff800-20260714-1610/determinism/run-a/itch50_order_events.ffir.json"
```

The second pair used a different working directory, absolute inputs, and `../`
outputs:

```sh
cd "/tmp/feedforge-ff800-20260714-1610/determinism/run-b/cwd"
"/tmp/feedforge-ff800-20260714-1610/release/src/feedforgec/feedforgec" \
  compile \
  --schema "/Users/rmbk/Documents/GitHub/FeedForge/schemas/nasdaq/totalview_itch_5_0.toml" \
  --pipeline "/Users/rmbk/Documents/GitHub/FeedForge/pipelines/all_messages.toml" \
  --output "../itch50_all.hpp" \
  --dump-ir "../itch50_all.ffir.json"
"/tmp/feedforge-ff800-20260714-1610/release/src/feedforgec/feedforgec" \
  compile \
  --schema "/Users/rmbk/Documents/GitHub/FeedForge/schemas/nasdaq/totalview_itch_5_0.toml" \
  --pipeline "/Users/rmbk/Documents/GitHub/FeedForge/pipelines/order_events.toml" \
  --output "../itch50_order_events.hpp" \
  --dump-ir "../itch50_order_events.ffir.json"
```

From the repository root:

```sh
shasum -a 256 \
  generated/include/feedforge/generated/nasdaq/itch50_all.hpp \
  "/tmp/feedforge-ff800-20260714-1610/determinism/run-a/itch50_all.hpp" \
  "/tmp/feedforge-ff800-20260714-1610/determinism/run-b/itch50_all.hpp" \
  generated/include/feedforge/generated/nasdaq/itch50_order_events.hpp \
  "/tmp/feedforge-ff800-20260714-1610/determinism/run-a/itch50_order_events.hpp" \
  "/tmp/feedforge-ff800-20260714-1610/determinism/run-b/itch50_order_events.hpp" \
  "/tmp/feedforge-ff800-20260714-1610/determinism/run-a/itch50_all.ffir.json" \
  "/tmp/feedforge-ff800-20260714-1610/determinism/run-b/itch50_all.ffir.json" \
  "/tmp/feedforge-ff800-20260714-1610/determinism/run-a/itch50_order_events.ffir.json" \
  "/tmp/feedforge-ff800-20260714-1610/determinism/run-b/itch50_order_events.ffir.json"
cmp -s generated/include/feedforge/generated/nasdaq/itch50_all.hpp \
  "/tmp/feedforge-ff800-20260714-1610/determinism/run-a/itch50_all.hpp"
cmp -s \
  "/tmp/feedforge-ff800-20260714-1610/determinism/run-a/itch50_all.hpp" \
  "/tmp/feedforge-ff800-20260714-1610/determinism/run-b/itch50_all.hpp"
cmp -s \
  generated/include/feedforge/generated/nasdaq/itch50_order_events.hpp \
  "/tmp/feedforge-ff800-20260714-1610/determinism/run-a/itch50_order_events.hpp"
cmp -s \
  "/tmp/feedforge-ff800-20260714-1610/determinism/run-a/itch50_order_events.hpp" \
  "/tmp/feedforge-ff800-20260714-1610/determinism/run-b/itch50_order_events.hpp"
cmp -s \
  "/tmp/feedforge-ff800-20260714-1610/determinism/run-a/itch50_all.ffir.json" \
  "/tmp/feedforge-ff800-20260714-1610/determinism/run-b/itch50_all.ffir.json"
cmp -s \
  "/tmp/feedforge-ff800-20260714-1610/determinism/run-a/itch50_order_events.ffir.json" \
  "/tmp/feedforge-ff800-20260714-1610/determinism/run-b/itch50_order_events.ffir.json"
```

Every comparison exited 0.

Initial canonical SHA-256:

| Artifact | SHA-256 |
|---|---|
| `itch50_all.hpp` — committed, run A, run B | `4173eecbd0b410353c5d4b333d70b782e915f109692b1db8e8a7056ff29febfc` |
| `itch50_order_events.hpp` — committed, run A, run B | `77791b95da79185d6f2b8b0c1deed1556dc5601fc1d285d7712f740f1da2f3ec` |
| `itch50_all.ffir.json` — run A and run B | `e34f980d60d75493bb808a9fdbdc943f4afc256858ae5e5899a13d632074332c` |
| `itch50_order_events.ffir.json` — run A and run B | `5789f4149bf6cf044239f138362677d1be9473ddc16196100dd59c04d49b641c` |

Both generated headers contain the compiler version, FFIR version, schema and
pipeline fingerprints, profile/variant ID, runtime API check, and `DO NOT
EDIT`. A separate static scan found:

- no `/Users/`, `/home/`, Windows absolute path, generation-time, hostname,
  username, or random-ID metadata;
- only standard-library and public `feedforge/` includes; and
- no `new`, `delete`, exceptions, RTTI, locks, packed structs, `std::vector`,
  owning `std::string`, or `std::function`.

The schema fingerprint is
`5caf2a24f113157cc5e74069339801fd332582dea234e66dd34148a8f12b938a`.
Pipeline fingerprints are
`42f71275830db9a05233c775df3b25b889a5fb13fc72485fa7819201b6a9c5ca`
and
`5091875ac081f047b55ccf8c8231b7aca268e4163d28e59956685944b1403ec1`.

## Schema, fixture, pipeline, and source-lock audit

A Python 3.14 `tomllib` audit independently asserted:

- the exact ordered 23-message discriminator/name/size inventory from SPEC
  §15;
- complete one-time coverage of every byte in every schema message;
- discriminator offset 0, width 1, role, and value;
- field citations;
- exactly 23 fixture manifests;
- raw hex size and discriminator consistency;
- `review_status = "approved"` and a review date;
- `author != reviewer` for every fixture;
- deterministic size-minus-one and size-plus-one error metadata;
- exact canonical all-message and order-event pipeline inventories; and
- source-lock size/SHA/status/date consistency against fresh downloads.

Result:

```text
PASS: 23-message schema inventory, byte coverage, discriminators, and citations
PASS: 23 approved fixture manifests; distinct author/reviewer; +/-1 invalid cases
PASS: canonical pipeline inventories and projection selectors
PASS: source lock metadata matches freshly downloaded PDF sizes and SHA-256 values
```

The official documents were fetched anew with:

```sh
mkdir "/tmp/feedforge-ff800-20260714-1610/sources"
curl --fail --location --silent --show-error \
  "https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf" \
  --output "/tmp/feedforge-ff800-20260714-1610/sources/NQTVITCHSpecification.pdf"
curl --fail --location --silent --show-error \
  "https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/binaryfile.pdf" \
  --output "/tmp/feedforge-ff800-20260714-1610/sources/binaryfile.pdf"
shasum -a 256 \
  "/tmp/feedforge-ff800-20260714-1610/sources/NQTVITCHSpecification.pdf" \
  "/tmp/feedforge-ff800-20260714-1610/sources/binaryfile.pdf"
wc -c \
  "/tmp/feedforge-ff800-20260714-1610/sources/NQTVITCHSpecification.pdf" \
  "/tmp/feedforge-ff800-20260714-1610/sources/binaryfile.pdf"
```

| Locked source | Fresh bytes | Fresh SHA-256 | Result |
|---|---:|---|---|
| Nasdaq TotalView-ITCH 5.0 | 1,200,722 | `45e0531d1b4b3beb886e9618b2ab824a5aa9bda3a99c0dff03509306e68aacc3` | Matches |
| Nasdaq BinaryFILE 1.00 | 84,384 | `a1f443400728b3ce44953e9ae263e4846fe6ad68420e7a635829872aefdfff60` | Matches |

This verifies the lock and audit inventory; it is not exchange certification.

## Corpus determinism

`fuzz/generate_corpus.cmake` was run twice into separate empty output
directories using the reviewed fixture directory and committed error seeds.
Relative names and file SHA-256 values were compared, and an aggregate digest
was computed over sorted `relative-name + NUL + bytes + NUL` entries.

```sh
cmake \
  -DFIXTURE_DIR="/Users/rmbk/Documents/GitHub/FeedForge/tests/fixtures/itch50" \
  -DSOURCE_CORPUS_DIR="/Users/rmbk/Documents/GitHub/FeedForge/fuzz/corpus" \
  -DOUTPUT_DIR="/tmp/feedforge-ff800-20260714-1610/corpus-a" \
  -P fuzz/generate_corpus.cmake
cmake \
  -DFIXTURE_DIR="/Users/rmbk/Documents/GitHub/FeedForge/tests/fixtures/itch50" \
  -DSOURCE_CORPUS_DIR="/Users/rmbk/Documents/GitHub/FeedForge/fuzz/corpus" \
  -DOUTPUT_DIR="/tmp/feedforge-ff800-20260714-1610/corpus-b" \
  -P fuzz/generate_corpus.cmake
diff -qr \
  "/tmp/feedforge-ff800-20260714-1610/corpus-a" \
  "/tmp/feedforge-ff800-20260714-1610/corpus-b"
```

Result: all three commands exited 0; `diff` produced no output. **93 files
were byte-identical** across both generations:
`MANIFEST.txt=1`, `binary_file=32`, `decode_one=26`, `replay=34`. Aggregate
SHA-256:
`fa27605ca92b023acf0f6186e11ecfd2b76fadd77d0806c120c85cdad787b2d8`.

## Documentation and example audit

README and every SPEC §23 document were checked against current public headers,
generated namespaces, CMake targets, CLI, statuses, and replay semantics.

- README says **offline checked v0.1**, experimental, not exchange-certified,
  and not production trading infrastructure.
- No unsupported latency, throughput, production-readiness, or comparative
  performance claim was found.
- The five-minute flow uses actual target/executable names.

The complete README sequence was run verbatim in an isolated copy of the dirty
working tree at `/tmp/feedforge-ff800-20260714-1610/readme-tree`:

```sh
mkdir "/tmp/feedforge-ff800-20260714-1610/readme-tree"
rsync -a --exclude=.git --exclude=build --exclude=.DS_Store ./ \
  "/tmp/feedforge-ff800-20260714-1610/readme-tree/"
cd "/tmp/feedforge-ff800-20260714-1610/readme-tree"
cmake --preset dev
cmake --build --preset dev --target regenerate
cmake --build --preset dev
cmake --build --preset dev --target check-generated
printf '\000\000' > build/dev/empty-complete.binaryfile
build/dev/examples/feedforge-replay build/dev/empty-complete.binaryfile
```

Result: all commands exited 0 in 9.36 seconds; canonical headers regenerated,
`check-generated` passed, and replay printed:

```text
status=complete frames_seen=0 events_emitted=0 known_messages_skipped=0 unknown_messages_skipped=0 bytes_consumed=2
```

This proves the commands against the audited working-tree content. It is not
the required clean-clone-after-commit evidence.

Required independent documentation exists for architecture, schema format,
pipeline format, generated API, testing/fixture provenance, and the future
backend seam. README links those documents, the schema audit, traceability,
this audit, and release notes.

Final documentation checks:

```sh
command -v markdownlint-cli2 || command -v markdownlint || true
git diff --cached --check
git diff --check
```

No standalone Markdown linter is installed; none was downloaded. Both Git diff
checks exited 0. IDE diagnostics reported no errors in the four FF-800-edited
files. A Python Markdown check over README, release notes, and all ten
`docs/*.md` files asserted newline-at-EOF, no trailing whitespace/tabs,
balanced code fences, and resolvable local links. It exited 0 with:

```text
PASS: 12 Markdown files have clean whitespace/fences and resolvable local links
```

## Required follow-up before v0.1.0

1. Commit the complete intended release tree.
2. Clone that exact commit into a new directory and run the documented
   `fast`, `dev` or `release`, compiler-off/install, and consumer flows.
3. Push the commit and require the full GitHub Actions matrix and fuzz-smoke
   workflow to pass. Record any Tier 2 waiver with every item required by SPEC
   §22; Tier 1 and fuzz gates cannot be waived.
4. Re-run documentation link/whitespace checks and update this record with the
   final commit and generated hashes.

Until all four steps are complete, SPEC §26 must remain unchecked and v0.1.0 is
not release-ready.
