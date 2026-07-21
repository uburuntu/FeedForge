# FeedForge v0.4.0

FeedForge v0.4.0 hardens the offline compiler and makes native Windows
generation a tested release surface. FeedForge remains experimental, is not
exchange-certified, and is not production trading infrastructure.

## Highlights

- Compiler output now uses exclusively created same-directory temporary files,
  explicit flush and close checks, atomic destination replacement, and RAII
  cleanup on POSIX and Windows.
- Three compiler fuzz targets exercise schema parsing and validation, pipeline
  parsing and resolution, and full lowering plus deterministic FFIR/C++
  emission using reviewed seed corpora.
- A malformed table-header preflight prevents a reachable toml++ parser
  assertion while preserving valid quoted, dotted, comment, and multiline
  string forms.
- Native MSVC 19.38 or newer now builds `feedforgec`, runs the full compiler and
  runtime suite, and reproduces canonical generated headers byte for byte.
  ClangCL remains an independent Windows compiler portability gate.

## Compatibility

The package and generated-header identity are `0.4.0`. Runtime API epoch 1,
revision 0 is unchanged because this release changes the host compiler and its
validation surface, not generated/runtime source compatibility. Schema,
pipeline, and FFIR format versions remain 1; `portable_checked.v1` remains the
only implementation profile.

Installed CMake packages continue to use same-minor compatibility before 1.0.
Regenerating custom headers is recommended so their compiler provenance records
v0.4.0, but v0.3 generated headers remain compatible with the v0.4 runtime API.

Runtime and generated headers remain strict C++20. The host compiler requires
C++23 with GCC 13.2, Clang 17, or MSVC 19.38 and a corresponding standard
library, or newer.

## Limitations

- Atomic replacement prevents partial normal-operation writes; it is not a
  filesystem transaction or a guarantee against storage loss after power
  failure.
- FeedForge provides no live networking, packet recovery, sequencing, order
  book, strategy, capture service, database, or operational trading controls.
- Exchange data is not bundled. Authoritative protocol documents remain
  subject to their owners' terms.

See the [v0.3.0 GitHub Release](https://github.com/uburuntu/FeedForge/releases/tag/v0.3.0)
for the previous release notes and validation evidence.
