# Compatibility

FeedForge versions the installable package, generated headers, schema and
pipeline formats, FFIR, implementation profile, runtime source API,
conformance bundle, and benchmark contract separately. They are related, but
none is a substitute for another.

## Package versions

FeedForge follows semantic versioning. Starting with 1.0.0, a minor or patch
release in the 1.x series preserves the documented public C++ source, CMake,
and compiler CLI compatibility described below. Additive APIs, targets,
options, commands, diagnostics, and input capabilities may appear in a minor
release. A documented 1.x surface is not removed or changed incompatibly until
a new package major version.

The installed package uses CMake's same-major version compatibility. A
non-`EXACT` request for a 1.x version can select an installed 1.x version that
is at least the requested version; it cannot select 0.x or a later major
version. `EXACT` requests remain exact. Pre-1.0 releases had no 1.x source
compatibility promise.

FeedForge does not promise a stable binary ABI, including between patch
releases. Rebuild consumers and regenerate custom headers when changing package
versions. The source compatibility policy does not make objects or libraries
built by different compilers, standard libraries, build modes, or FeedForge
versions link-compatible.

## Public C++ source compatibility

The 1.x source policy covers documented names and behavior in installed headers
under `include/feedforge`, the exported `FeedForge::runtime` target, and the two
canonical generated targets. Existing well-formed consumer source that uses
those documented surfaces will continue to compile in later 1.x releases when
it is rebuilt with a supported toolchain. New names and capabilities may be
added only when they do not make existing documented use ill-formed. Existing
overload sets and enum or result domains are not treated as open unless their
documentation explicitly says they are extensible.

The policy does not cover implementation details, undocumented names, test
helpers, source-tree-only targets, generated declaration spelling beyond the
documented generated API, or C++ ABI. Correctness fixes may reject input that
was accepted contrary to a published format or protocol rule.

## CMake compatibility

Within 1.x, the documented imported target names, supported cache options, and
the `feedforge_generate()` call shape remain source-compatible. Existing valid
`find_package(FeedForge CONFIG ...)` integrations can move to a later compatible
1.x package without changing their CMake source. New targets, options, and
optional arguments may be added.

This promise applies to documented CMake behavior, not generated build-system
internals, build-tree paths, private target properties, or the developer
`Makefile`. CMake 3.25 is the declared configuration floor. Linux minimum
runtime/generated jobs pin CMake 3.25.2 and exercise configure, build, CTest,
install, and an external canonical-header consumer. The full exception-enabled
compiler job also pins 3.25.2 through build, CTest, and canonical regeneration.
Other platform and generator combinations use runner-provided CMake.

## Compiler CLI compatibility

Within 1.x, the documented `feedforgec validate`, `compile`, and `dump-ir`
commands, their required option names, `--help`, `--version`, and the exit-status
categories in [Compiler CLI](compiler-cli.md) remain automation-compatible.
Minor releases may add commands, optional arguments, and diagnostic codes.

Stable diagnostic codes retain their documented category and meaning. Human
readable diagnostic wording, source positions made more precise, help text,
compiler version output, and generated provenance may change. Automation must
use exit status and diagnostic code rather than matching complete prose. FFIR
JSON remains a deterministic diagnostic artifact, not a stable CLI interchange
format.

## Runtime and generated headers

The public runtime exposes two constants:

```cpp
feedforge::runtime_api_epoch
feedforge::runtime_api_revision
```

Every generated header records `required_runtime_api_epoch` and
`minimum_runtime_api_revision`. Compilation succeeds only when the epochs are
equal and the installed runtime revision is at least the generated header's
minimum. An epoch change is an intentional source-compatibility break. A
revision increase is additive within that epoch.

This check prevents known-incompatible runtime/header combinations; it is not
an ABI guarantee. Regenerating custom headers with the installed compiler is
the preferred upgrade path.

Package compatibility does not replace this compile-time check. A generated
header from another package version is usable only when its required runtime
epoch and minimum revision are satisfied and the documented generated API it
uses remains available.

## Other versioned contracts

- Schema and pipeline `format_version` values version their TOML grammars.
  Package 1.x compatibility does not imply that their format versions are 1.x.
- FFIR `format_version` versions the resolved model. Canonical FFIR JSON is a
  deterministic diagnostic artifact, not a long-term interchange promise.
- `portable_checked.v1` identifies the emitted implementation profile and its
  observable decode semantics.
- Schema and pipeline fingerprints identify resolved input semantics, not file
  spelling or paths.
- The benchmark contract is versioned independently. Contract 1.0.0 evidence
  cannot be compared with contract 2.0.0 evidence.
- The synthetic conformance bundle remains format version 1; its format is not
  coupled to the FeedForge package version.

Replay counters and offsets are `std::uint64_t`. Event field types remain
determined by the schema-to-C++ mapping in [Generated C++ API](generated-api.md).

## Toolchain qualification

The strict C++20 runtime and committed generated headers have release-blocking
minimum jobs for GCC 11 and Clang 14 on Linux. The C++23 host compiler is
release-blocking on current GCC, Clang 18 with libc++ 18, AppleClang on the
macOS 15 arm64 image, and the MSVC and ClangCL toolsets on the Windows 2022
image. Those are CI-qualified configurations, not evidence for the previously
claimed GCC 13.2 or Clang 17 numeric host-compiler floors. Native MSVC has an
explicit configure rejection bound of 19.38 (Visual Studio 2022 17.8), but that
exact lower bound is not a release-matrix job; qualification uses the toolset
currently supplied by the Windows 2022 image.

Other conforming C++20 or C++23 toolchains may work but are not part of the
supported floor unless added to the release-blocking matrix. The exact compiler
patch versions supplied by rolling hosted images are recorded in each CI run.
