# FF-700 hardening evidence

Recorded 2026-07-14 for the working tree implementing FF-700. This is
requirement-to-test evidence only; it does not claim that unrun hosted jobs are
green. The local host was macOS arm64 with AppleClang 21.0.0 and CMake 4.4.0.

## Traceability

- SPEC 21.5 malformed and status behavior:
  `integration.itch50_end_to_end` retains all 23 reviewed fixture, size-minus,
  size-plus, projection, lowercase `h`, current `O`, replay, and stop checks.
  `hardening.status_branches` exercises every error-mode canonical decode and
  replay terminal branch, including both cursor framing errors and strict
  trailing data. `hardening.unknown_skip` generates and executes a test-only
  unknown-skip pipeline without changing canonical pipelines.
- SPEC 21.6 structure and ordering:
  `compiler.generated_cpp20/23` count profile loads and prove zero loads before
  exact-size validation. Emitter tests assert absent unprojected members/loads.
  Canonical header probes assert absent projected members and compare every
  event's size/alignment between C++20 and C++23. Configure-time compile-fail
  cases cover missing, throwing, wrong-result, and ambiguous sinks.
- SPEC 21.7 allocation and build modes:
  `hardening.allocation` replaces all relevant global allocation forms and
  measures only fixed-input/fixed-sink decode or replay regions for emit, skip,
  complete, incomplete, and stop paths. `hardening.no_exceptions_rtti` is an
  explicit C++20 generated-runtime target. Configure-time assertions reject
  compile/link option leakage from exported interfaces. RTSan is
  compile/link/attribute probed and creates a private smoke target only when
  supported.
- SPEC 21.8 fuzzing:
  `fuzz_binary_file`, `fuzz_decode_one`, and `fuzz_replay` assert deterministic
  outcomes, validation-before-load, sink ordering/call bounds, and coherent
  summaries. Configure-time corpus generation validated all 23 reviewed
  `raw_hex`/`raw_size` pairs and produced 26 decode, 34 replay, and 32 framing
  seeds. The second independent generation was byte-identical.
- SPEC 22 release matrix:
  workflows define minimum GCC 11/Clang 14 compiler-off C++20, current
  GCC/Clang mixed and generated-C++23 jobs, ASan+UBSan, no-exception/RTTI,
  installed generation and runtime-only consumers, macOS arm64 AppleClang,
  Windows x64 MSVC Tier 2, optional RTSan, and all three fuzz smokes. Tier policy
  is explicit and no Tier 2 job is silently allowed to fail.

## Local command evidence

All builds below enabled `FEEDFORGE_WARNINGS_AS_ERRORS=ON`.

- `cmake --preset fast -DFEEDFORGE_WARNINGS_AS_ERRORS=ON && cmake --build
  --preset fast -j 4 && ctest --preset fast`: passed 11/11.
- `cmake --preset dev -DFEEDFORGE_WARNINGS_AS_ERRORS=ON && cmake --build
  --preset dev -j 4 && ctest --preset dev`: passed 25/25.
- `cmake --preset release -DFEEDFORGE_WARNINGS_AS_ERRORS=ON && cmake --build
  --preset release -j 4 && ctest --preset release`: passed 25/25.
- `cmake --preset compiler-off -DFEEDFORGE_WARNINGS_AS_ERRORS=ON && cmake
  --build --preset compiler-off -j 4 && ctest --preset compiler-off`: passed
  12/12, including the runtime-only installed canonical consumer.
- `cmake --preset no-exceptions-rtti -DFEEDFORGE_WARNINGS_AS_ERRORS=ON &&
  cmake --build --preset no-exceptions-rtti -j 4 && ctest --preset
  no-exceptions-rtti`: passed 1/1.
- `cmake --preset sanitizers -DFEEDFORGE_WARNINGS_AS_ERRORS=ON && cmake
  --build --preset sanitizers -j 4`: exit 0.
- `ASAN_OPTIONS=detect_leaks=0:abort_on_error=1
  UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 ctest --preset sanitizers`:
  passed 25/25.
- `ctest --test-dir build/dev --output-on-failure -L allocation`: passed 1/1.
- `ctest --test-dir build/dev --output-on-failure -L arbitrary-input`: passed
  3/3 standalone harnesses.
- `ctest --test-dir build/dev --output-on-failure -L installed`: passed 2/2
  (runtime-only canonical and compiler-enabled generated consumers).
- `cmake --build --preset dev --target check-generated`: exit 0; canonical
  generated headers were byte-identical and were not edited.
- `cmake -DFIXTURE_DIR="${PWD}/tests/fixtures/itch50"
  -DSOURCE_CORPUS_DIR="${PWD}/fuzz/corpus"
  -DOUTPUT_DIR="${PWD}/build/fuzz-corpus-local" -P
  fuzz/generate_corpus.cmake`, repeated with
  `OUTPUT_DIR="${PWD}/build/fuzz-corpus-repeat"`, then `diff -r
  build/fuzz-corpus-local build/fuzz-corpus-repeat`: exit 0.
- `cmake --preset rtsan -DFEEDFORGE_WARNINGS_AS_ERRORS=ON && cmake --build
  --preset rtsan -j 4 && ctest --test-dir build/rtsan --output-on-failure
  --no-tests=ignore -R '^hardening\.rtsan_smoke$'`: exit 0 with the optional
  test not registered on AppleClang.
- `git diff --check`: exit 0.

## Local limitations and CI-only evidence

- AppleClang does not ship a usable libFuzzer/ASan/UBSan fuzz runtime.
  `cmake --preset fuzz -DFEEDFORGE_WARNINGS_AS_ERRORS=ON` therefore exited 1
  with the intended actionable diagnostic. The three standalone
  arbitrary-input harnesses passed locally; actual fixed-duration libFuzzer
  runs are Linux CI-only.
- AppleClang is not upstream Clang, so the `rtsan` preset reported the optional
  smoke disabled, configured and built successfully, and registered no RTSan
  test. A supported upstream-Clang run is CI-only.
- Apple ASan reported that `detect_leaks=1` is unsupported and aborted before
  test code. The local ASan+UBSan suite passed with leak detection disabled;
  Linux CI keeps `detect_leaks=1`.
- GCC 11, Clang 14, current Linux GCC/Clang, hosted macOS, and Windows MSVC were
  not available locally. Their workflow definitions are not recorded as
  passes.

No SPEC conflict or semantic deviation was introduced. Audited schema,
canonical pipeline/fixture semantics and review metadata, compiler behavior,
runtime public semantics, and canonical generated headers were unchanged.
