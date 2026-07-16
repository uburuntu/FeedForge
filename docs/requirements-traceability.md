# v0.1 requirement-to-test traceability

This document maps the v0.1 Definition of Done and the major normative
requirements in SPEC Sections 3, 7, 10–23, and 26 to implementation and
evidence. The recorded status is for the uncommitted working tree audited on
2026-07-14, not for a release commit.

Status meanings:

- **Pass** — inspected implementation plus fresh local executable or static
  evidence satisfies the requirement within the available platform.
- **Pending external CI** — the required hosted platform/compiler or
  libFuzzer result is unavailable for this private, unpushed tree.
- **Pending clean-clone-after-commit** — the test requires the final committed
  revision and therefore cannot be represented by this dirty tree.
- **Fail** — contrary evidence exists. A fail is never converted to a pass by
  the presence of a target or unrun workflow.

Exact commands, environment, hashes, and outcomes are in
[the FF-800 release audit](release-audit.md).

## Current gate status

**Local recommendation: GO.** The two prior local failures are now **Pass**.
This is not a release pass: hosted CI, required Linux libFuzzer execution, and
the post-commit clean-clone check remain explicitly pending. Do not tag or
publish v0.1.0 until all three produce passing evidence.

## Major normative requirements

| SPEC requirement | Status | Implementation | Test or review evidence |
|---|---|---|---|
| §3: compilable/installable C++20 runtime | **Pass** | `include/feedforge/`, `CMakeLists.txt`, `cmake/FeedForgeConfig.cmake.in` | Fresh compiler-off build/install; `consumer.installed_canonical`; independent installed canonical consumer |
| §3: C++23 code-generator host tool | **Pass** | `src/feedforgec/`, `FeedForge::compiler` | Configure-time C++23 feature probe; fresh Release build; `compiler.version`, `compiler.help` |
| §3: versioned schema and pipeline parsers | **Pass** | `parse_toml.*`, `model.*`, `docs/schema-format.md`, `docs/pipeline-format.md` | `compiler.validate_schema`, `compiler.validate_pipeline`, `compiler.frontend` |
| §3, §15–16: complete current ITCH schema and canonical pipelines | **Pass** | `schemas/nasdaq/totalview_itch_5_0.toml`, `pipelines/*.toml`, `docs/schema-audit.md` | Independent 23-message/type/size/byte-coverage audit; `integration.itch50_end_to_end` |
| §3, §11: explicit versioned serialisable FFIR | **Pass** | `ir.*`, `lower.*`, `--dump-ir` | `compiler.frontend`, `compiler.emit_atomic_deterministic`; two cross-directory FFIR SHA comparisons |
| §3, §12–13: deterministic C++20 and one `portable_checked.v1` profile | **Pass** | `emit_cpp.*`, `profile/portable_checked.hpp`, canonical generated headers | `check-generated`; two independent header generations; `compiler.emitter`; profile-substitution semantics |
| §3, §18: zero-copy in-memory BinaryFILE cursor | **Pass** | `framing/binary_file.hpp` | `unit.binary_file`, arbitrary-input smoke, allocation test |
| §3, §19: owning projected events, static allocation-free decoder, typed stop-capable sink | **Pass** | generated headers, `flow.hpp`, `result.hpp`, profile concepts | `compiler.generated_cpp20/23`, compile-fail sink tests, `hardening.status_branches`, `hardening.allocation` |
| §3, §21: exact positive and wrong-size fixtures for all messages | **Pass** | 23 `tests/fixtures/itch50/*.toml` manifests | Distinct approved author/reviewer metadata; per-message valid, size-minus-one, and size-plus-one tests |
| §3, §21.8: sanitizer and fuzz targets | **Pass** for target/corpus presence; execution tracked separately | `fuzz/`, sanitizer options, `.github/workflows/fuzz-smoke.yml` | ASan+UBSan suite passed locally; deterministic 93-file corpus passed; actual libFuzzer smoke is **Pending external CI** |
| §3, §22: supported CI matrix | **Pending external CI** | `.github/workflows/ci.yml`, `.github/workflows/fuzz-smoke.yml` | Workflows inspected, but this private unpushed tree has no run |
| §3: end-to-end projected replay example | **Pass** | `examples/replay_binary_file.cpp`, `feedforge-replay` | Fresh build and empty-complete BinaryFILE run; end-to-end 23-message replay test |
| §3, §13: deliberate seam without premature kernel variants | **Pass** | one emitted profile, `decoder_implementation` concept, `docs/adding-a-backend.md` | Static inventory; synthetic alternate implementation test |
| §7.1: C++23 host, strict C++20 runtime/generated, C++23 compatibility | **Pass** locally | Per-target compile features and `CXX_EXTENSIONS OFF` | C++20/C++23 generated builds plus layout comparison; compiler feature probe |
| §7.2, §22: minimum compilers and supported OS jobs | **Pending external CI** | CI matrix and compiler-off preset | Local AppleClang arm64 passes; Linux GCC/Clang minimum/current and Windows MSVC require hosted runs |
| §7.3: no exceptions/RTTI, `noexcept`, no post-setup allocation | **Pass** locally | hot APIs and generated calls are `noexcept` | `hardening.no_exceptions_rtti`, `hardening.allocation`, generated static assertions |
| §7.4: portable baseline without packed/unaligned/ISA-specific wire access | **Pass** | explicit `std::byte` shift loads | Runtime tests plus static generated/runtime scan |
| §10.1: CLI, exit classes, explicit inputs, no network/environment config, atomic sibling output | **Pass** | `src/feedforgec/main.cpp` | CLI/atomic tests plus static host-tool API scan; invalid input preserves destinations |
| §10.2: one pinned TOML dependency private to host compiler | **Pass** | exact `toml++` version/commit, `docs/adr/0001-tomlplusplus.md` | Installed runtime target inspection and compiler-off consumer show no dependency |
| §10.3–10.4: stable actionable diagnostics and exact name rules | **Pass** | `diagnostics.*`, parser/model validation | `compiler.validation_identifiers_diagnostics` covers every source-name scope, all recognized C++ keywords/reserved forms, object/text/path stability; CLI negative tests |
| §11: canonical ordering, semantic fingerprints, no path/time in FFIR | **Pass** | `canonical_json`, SHA-256 implementation | Published SHA vectors; reordered fixtures; cross-directory identical FFIR hashes |
| §12: byte-deterministic generated source and required provenance | **Pass** | generated preamble and runtime API assertion | Exact `0.1.0` provenance; `check-generated`; committed/run-A/run-B hashes match; prohibited metadata scan |
| §14: closed schema grammar and every validation rule | **Pass** | Parser/model checks plus split focused tests | `compiler.validation_schema_grammar`, `.validation_schema_types`, `.validation_schema_layout`, and `.validation_identifiers_diagnostics` cover every individual grammar/validation rule and boundary with stable code/useful-text assertions |
| §14.3: allowed alpha metadata never causes runtime semantic rejection | **Pass** | metadata is omitted from decode predicates | Generated semantics preserve an unlisted `Z`; schema audit records the policy |
| §15: all 23 official fields, lowercase `h`, current `O` | **Pass** | schema and field-by-field audit | Independent inventory/coverage script; all-message fixture replay |
| §16: closed pipeline grammar and exact canonical projections | **Pass** | both pipeline TOMLs and `validate_pipeline` | `compiler.validation_pipeline_grammar` and `.validation_pipeline_semantics`; independent pipeline inventory; projection tests |
| §17: portable loads, exact owning value types, stable result semantics | **Pass** | `wire/`, `types/`, `result.hpp` | `unit.runtime_primitives`, generated semantics and trait assertions |
| §18: distinct BinaryFILE states, exact offsets, sticky terminal state | **Pass** | `binary_file_cursor` | `unit.binary_file`, `hardening.status_branches`, replay integration |
| §19.1–19.3: exact projected events and validate-before-load decoder | **Pass** | generated event/decoder source | emitter structural tests, counted-load implementation, end-to-end fixtures, compile-fail tests |
| §19.3: known unselected messages pass exact-size validation before skip | **Pass** | generated known-size switch precedes selected-event switch | all 15 valid unselected fixtures skip; malformed known-unselected `H` errors with zero loads |
| §19.4: strict replay statuses, counters, offsets, and stop ordering | **Pass** | generated `replay_binary_file` and runtime summary | end-to-end replay, status-branch, BinaryFILE, and allocation tests |
| §19.5: generated compile-time checks and runtime API compatibility | **Pass** | generated `static_assert`s and metadata | emitter tests and C++20/C++23 canonical-header builds |
| §20: exported targets, package config, `feedforge_generate`, options/presets | **Pass** locally | root/CMake modules and presets | source-tree generation; installed canonical and custom-generation consumers |
| §20: no warning/sanitizer/exception/optimization flag leakage | **Pass** | private project-target options | installed interface-property checks and external consumer builds |
| §21.1: independent fixture provenance | **Pass** | fixture metadata and `docs/schema-audit.md` | 23 approved manifests; author `schema-author-agent` differs from reviewer `schema-review-agent`; fresh official-source hash match |
| §21.2, §21.4–21.7: required runtime/framing/conformance/projection/hardening tests | **Pass** locally | `tests/unit`, `integration`, `golden`, `hardening` | Current Release 31/31 plus prior fresh ASan+UBSan 25/25; all present tests passed |
| §21.3: every schema/pipeline validation rule has a focused test | **Pass** | Six `tests/compiler/validation_*.cpp` executables | Fresh focused re-audit: all six split validation executables passed; complete `compiler.*` family passed 15/15; exact codes, useful category text, object paths, and normalized source paths asserted |
| §21.8: three libFuzzer targets and complete seed classes | **Pending external CI** for libFuzzer execution | `fuzz_binary_file`, `fuzz_decode_one`, `fuzz_replay`, generated corpora | Corpus generation and standalone harnesses pass; local AppleClang reports no usable libFuzzer runtime |
| §21.9: installed generated consumer | **Pass** locally | `tests/consumer`, `run_installed.cmake` | `consumer.installed_generated` configures, generates, builds, and runs |
| §22: Tier 1/Tier 2 hosted jobs and fuzz smoke | **Pending external CI** | CI workflow definitions | No run exists for the uncommitted implementation |
| §22: hosted CI publishes no latency or throughput comparison | **Pass** | correctness-only workflow steps | Workflow/documentation claim scan found no benchmark or performance publication |
| §23: independent architecture/formats/API/testing/backend docs and limitations | **Pass** | `README.md`, Section 23 docs, release docs | Link/API/claim audit; local link and whitespace checks |

## Definition of Done

| SPEC §26 statement | Status | Concrete evidence |
|---|---|---|
| A clean clone configures, builds, and tests with documented presets. | **Pending clean-clone-after-commit** | Fresh out-of-tree builds pass, but a dirty uncommitted tree cannot prove a clone of the final revision. |
| Runtime/generated public targets have zero third-party dependencies. | **Pass** | Installed `FeedForge::runtime` has only C++20/includes; canonical targets link only `FeedForge::runtime`; generated includes are standard or public FeedForge headers. |
| `feedforgec` builds as C++23 and can be disabled. | **Pass** | C++23 probe/build passes; compiler-off configure/build/install passes. |
| A compiler-disabled C++20 build installs runtime and canonical pipelines. | **Pass** | Fresh compiler-off install and independent canonical consumer pass. |
| Schema/pipeline validation emits actionable stable diagnostics. | **Pass** | Exact-code parser/model/CLI negative tests pass. |
| FFIR and generated C++ are byte-deterministic. | **Pass** | Two cross-directory runs produce identical FFIR and header SHA-256 values; committed headers also match. |
| Generated provenance has version/fingerprints but no host path/time. | **Pass** | Runtime/compiler/package/generated identity is exactly `0.1.0`; runtime API remains `1`; required metadata and prohibited-data checks pass. |
| The complete 23-message current ITCH 5.0 schema passes field audit. | **Pass** | Field-by-field schema audit plus independent inventory, size, citation, discriminator, and byte-coverage checks. |
| Every message has independently reviewed valid and invalid fixtures. | **Pass** | 23 approved manifests have distinct author/reviewer identities and reviewed valid plus deterministic ±1-size cases. |
| The checked decoder validates exact size for every known message. | **Pass** | All 23 valid/short/long tests plus counted zero-load failures. |
| Known unselected messages are validated before being skipped. | **Pass** | Every unselected fixture skips only at valid size; malformed `H` errors with zero loads/sink calls. |
| Selected messages load only projected fields into owning trivial events. | **Pass** | Emitter structural checks, projected event compile checks, exact fixture values, and generated type-trait assertions. |
| BinaryFILE complete, incomplete, malformed, and stopped states are distinct. | **Pass** | BinaryFILE and replay status-branch tests. |
| Replay and a no-op sink allocate zero memory after setup. | **Pass** | `hardening.allocation` passes all measured paths. |
| Runtime/generated code builds without exceptions and RTTI. | **Pass** | Explicit `-fno-exceptions -fno-rtti` target passes. |
| Required C++20, C++23, Linux, macOS Arm, and Tier 2 jobs meet policy. | **Pending external CI** | Local macOS arm64 C++20/C++23 passes; required hosted Linux/compiler and Windows jobs are unrun. |
| ASan, UBSan, and fuzz smoke are green. | **Pending external CI** | Fresh local ASan+UBSan 25/25 passes; actual libFuzzer is unavailable locally and the required Linux fuzz workflow is unrun. |
| Canonical generated packages pass `check-generated`. | **Pass** | Fresh Release `check-generated` exits 0; independent hashes match committed files. |
| Source-tree and installed external consumer examples work. | **Pass** | Source-tree generation and both installed canonical/custom-generation consumers pass. |
| README makes no unsupported production or latency claim. | **Pass** | README states experimental, non-certified, non-production scope and contains no performance claim. |
| No optimisation-only dependency or architecture-specific hot-path branch entered v0.1. | **Pass** | Static dependency/source review finds one portable shift-load profile and no ISA-specific baseline branch. |

## Resolved local release blockers

### Focused compiler-validation coverage — resolved

Six split CTest executables now provide direct positive/negative evidence for
every rule in SPEC §§10.4, 14.2–14.3, and 16.2–16.3. This includes the original
gaps (`65536`, missing decimal scale, discriminator role without value, and
nested unknown/required keys) plus the complete identifier, type compatibility,
layout/coverage, duplicate, wildcard/explicit, schema/profile/policy, and
diagnostic source/path matrices. Every invalid category asserts its exact
diagnostic code and category-specific useful text. No parser fix or requirement
exception was needed. A fresh isolated re-audit passed the complete
`compiler.*` family **15/15**.

### Consistent v0.1.0 release identity — resolved

The root project/package, public `feedforge::version_string`, exact compiler
CLI output, FFIR/golden provenance, and both compiler-regenerated canonical
headers now identify `0.1.0`. Compile-time tests confirm
`feedforge::runtime_api_version == 1` and both canonical generated headers
require runtime API `1`. Independent relative/absolute-input generations match
the committed canonical headers; exact hashes are in the release audit.

Hosted CI, required Linux libFuzzer execution, and a clean clone of the final
commit remain pending. These statuses were not promoted by local evidence.
