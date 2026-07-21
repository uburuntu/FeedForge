# FeedForge v0.5.0

FeedForge v0.5.0 adds reproducible interoperability artifacts and a validated
local vcpkg adoption path. FeedForge remains experimental, is not
exchange-certified, and is not production trading infrastructure.

## Highlights

- A deterministic synthetic conformance bundle packages all 23 independently
  reviewed Nasdaq TotalView-ITCH 5.0 fixture payloads, 48 invalid-payload
  cases, and seven BinaryFILE framing cases with normalized expectations.
- Expanded bundle directories, `.tar.gz` archives, and `.zip` archives carry
  byte hashes, source and fixture provenance, and reproducible member metadata.
  Contract tests validate every published expectation and provenance record.
- A repository-owned vcpkg overlay installs the C++20 runtime and canonical
  generated headers by default. Its native-only `compiler` feature adds the
  C++23 host compiler and the pinned `tomlplusplus` dependency.
- Hosted CI bootstraps a pinned vcpkg revision with binary caching disabled and
  builds separate runtime-only and compiler-enabled external consumers.
- The compiler can select the exception-enabled toml++ ABI required by the
  vcpkg dependency while retaining the bounded TOML nesting policy. The normal
  source build remains exception-disabled by default.

## Compatibility

The package and generated-header identity are `0.5.0`. Runtime API epoch 1,
revision 0 is unchanged because the release adds packaging and conformance
surfaces without changing generated/runtime source compatibility. Schema,
pipeline, and FFIR format versions remain 1; `portable_checked.v1` remains the
only implementation profile. The conformance bundle has its own format version
1 and is not coupled to the package minor version.

Installed CMake packages continue to use same-minor compatibility before 1.0.
Regenerating custom headers is recommended so their compiler provenance records
v0.5.0, but v0.4 generated headers remain compatible with the v0.5 runtime API.

Runtime and generated headers remain strict C++20. The host compiler requires
C++23 with GCC 13.2, Clang 17, or MSVC 19.38 and a corresponding standard
library, or newer. Python 3.11 or newer is required only for conformance bundle
generation and verification.

## Limitations

- The local vcpkg overlay builds from its containing checkout and is not a
  registry release. Pin that checkout and keep `VCPKG_BINARY_SOURCES=clear` to
  avoid reusing a package built from different source bytes.
- Conformance payloads are synthetic interoperability fixtures, not captured
  exchange data or exchange certification. The v0.5 GitHub release contains
  the deterministic FeedForge source archives; conformance archives are
  generated locally and are not separate release assets.
- FeedForge provides no live networking, packet recovery, sequencing, order
  book, strategy, capture service, database, or operational trading controls.

See the [v0.4.0 GitHub Release](https://github.com/uburuntu/FeedForge/releases/tag/v0.4.0)
for the previous release notes and validation evidence.
