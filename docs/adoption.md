# Adopting FeedForge

FeedForge provides a C++20 runtime and committed generated headers. Custom
pipeline generation additionally builds a C++23 host compiler. FeedForge is
experimental, is not exchange-certified, and does not provide live networking,
recovery, order-book reconstruction, or production-trading guarantees.

## Build and install the package

From a FeedForge source checkout, build a compiler-enabled package:

```sh
cmake -S . -B build/adoption \
  -DCMAKE_BUILD_TYPE=Release \
  -DFEEDFORGE_BUILD_COMPILER=ON \
  -DFEEDFORGE_BUILD_TESTS=OFF \
  -DFEEDFORGE_BUILD_EXAMPLES=OFF
cmake --build build/adoption --config Release
cmake --install build/adoption --config Release \
  --prefix build/feedforge-install
```

Canonical consumers do not need the compiler, C++23, or toml++. For a
runtime-only package, use a separate build:

```sh
cmake -S . -B build/adoption-runtime \
  -DCMAKE_BUILD_TYPE=Release \
  -DFEEDFORGE_BUILD_COMPILER=OFF \
  -DFEEDFORGE_BUILD_TESTS=OFF \
  -DFEEDFORGE_BUILD_EXAMPLES=OFF
cmake --build build/adoption-runtime --config Release
cmake --install build/adoption-runtime --config Release \
  --prefix build/feedforge-runtime-install
```

After `find_package(FeedForge CONFIG REQUIRED)`, package data is available at:

- `FeedForge_SCHEMA_DIR` for compiler schemas;
- `FeedForge_DOC_DIR` for this guide and the Section 23 documentation; and
- `FeedForge_SKILL_DIR` for the optional Agent Skills.

The documentation and skills are package data only. Canonical targets do not
read them at configure time, build time, or runtime.

## Canonical C++20 consumer

Choose one committed target:

- `FeedForge::generated::itch50_order_events` with
  `<feedforge/generated/nasdaq/itch50_order_events.hpp>`; or
- `FeedForge::generated::itch50_all` with
  `<feedforge/generated/nasdaq/itch50_all.hpp>`.

Minimal `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.25)
project(FeedForgeConsumer LANGUAGES CXX)

find_package(FeedForge CONFIG REQUIRED)

add_executable(feedforge_consumer main.cpp)
target_compile_features(feedforge_consumer PRIVATE cxx_std_20)
set_target_properties(feedforge_consumer PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(
  feedforge_consumer
  PRIVATE FeedForge::generated::itch50_order_events
)
```

Minimal `main.cpp` with every required typed sink overload:

```cpp
#include <feedforge/generated/nasdaq/itch50_order_events.hpp>

#include <array>
#include <cstddef>

namespace events =
    feedforge::generated::nasdaq::itch50_order_events;

struct sink {
  feedforge::flow operator()(events::add_order const&) noexcept {
    return accept();
  }
  feedforge::flow operator()(events::add_order_mpid const&) noexcept {
    return accept();
  }
  feedforge::flow operator()(events::order_executed const&) noexcept {
    return accept();
  }
  feedforge::flow operator()(
      events::order_executed_with_price const&) noexcept {
    return accept();
  }
  feedforge::flow operator()(events::order_cancel const&) noexcept {
    return accept();
  }
  feedforge::flow operator()(events::order_delete const&) noexcept {
    return accept();
  }
  feedforge::flow operator()(events::order_replace const&) noexcept {
    return accept();
  }
  feedforge::flow operator()(events::trade const&) noexcept {
    return accept();
  }

  unsigned calls{};

 private:
  feedforge::flow accept() noexcept {
    ++calls;
    return feedforge::flow::continue_;
  }
};

static_assert(events::sink_for_all_selected_events<sink>);

int main() {
  constexpr std::array input{std::byte{0}, std::byte{0}};
  sink destination;
  const feedforge::replay_summary result =
      events::replay_binary_file(input, destination);
  return result.status == feedforge::replay_status::complete ? 0 : 1;
}
```

The source checkout's copyable template is under `examples/consumer-template`;
an installed copy is under
`${FeedForge_DOC_DIR}/examples/consumer-template`. Configure, build, and run the
source copy from the FeedForge checkout's parent directory:

```sh
cmake -S FeedForge/examples/consumer-template \
  -B build/feedforge-consumer \
  -DCMAKE_PREFIX_PATH="$PWD/FeedForge/build/feedforge-runtime-install"
cmake --build build/feedforge-consumer
cmake --build build/feedforge-consumer --target run-feedforge-consumer
```

For real BinaryFILE input, load or map the file before replay and keep its byte
span alive for the entire call. An event reference is valid only during its sink
overload; copy the owning event if it must survive the callback.

## Package, FetchContent, and subdirectory acquisition

An installed config package is the shortest and most reproducible integration
path. No component selection or manual include directory is required.

For a pinned source dependency, `FetchContent` is also sufficient:

```cmake
include(FetchContent)
FetchContent_Declare(
  feedforge
  GIT_REPOSITORY https://github.com/uburuntu/FeedForge.git
  GIT_TAG <immutable-reviewed-revision>
)
FetchContent_MakeAvailable(feedforge)

target_link_libraries(
  feedforge_consumer
  PRIVATE FeedForge::generated::itch50_order_events
)
```

When FeedForge is not the top-level project, compiler, tests, and examples
default to `OFF`; fuzzers and benchmarks are always opt-in. A canonical
FetchContent or `add_subdirectory()` consumer therefore does not configure
toml++, run a C++23 feature check, or build the host compiler. For custom
generation, set `FEEDFORGE_BUILD_COMPILER` to `ON` before making the source
available. `add_subdirectory(/path/to/FeedForge feedforge)` has the same target
and option behavior.

## Custom pipeline

Use custom generation only when the committed projections do not fit. Example
`consumer/custom_events.toml`:

```toml
format_version = 1
name = "custom_events"
namespace = "example::custom_events"
schema = "nasdaq_totalview_itch_5_0"
profile = "portable_checked"
unknown_messages = "error"
unselected_messages = "skip"

[[emit]]
source = "S"
event = "system_event"
fields = ["stock_locate", "timestamp", "event_code"]
```

Validate it with the compiler-enabled install:

```sh
build/feedforge-install/bin/feedforgec validate \
  --schema schemas/nasdaq/totalview_itch_5_0.toml \
  --pipeline consumer/custom_events.toml
```

Generate under the consumer build tree:

```cmake
find_package(FeedForge CONFIG REQUIRED)
if(NOT TARGET FeedForge::compiler)
  message(FATAL_ERROR "Custom generation needs a compiler-enabled package")
endif()

feedforge_generate(
  NAME custom_events
  SCHEMA nasdaq_totalview_itch_5_0
  PIPELINE "${CMAKE_CURRENT_SOURCE_DIR}/custom_events.toml"
)
target_link_libraries(
  feedforge_consumer
  PRIVATE FeedForge::generated::custom_events
)
```

Include `<feedforge/generated/custom_events.hpp>` and use namespace
`example::custom_events`. Each explicit `fields` list preserves its order and
the schema's strong C++ types. Pipeline TOML cannot contain C++, computed or
renamed fields, expressions, changed wire layout, or an unselected-message
policy other than `"skip"`.

### Custom output contract

Omitting `OUTPUT` is the simplest path and always creates
`<feedforge/generated/NAME.hpp>` under a target-specific build directory.

An explicit `OUTPUT` is resolved relative to `CMAKE_CURRENT_BINARY_DIR` and
must remain inside `CMAKE_BINARY_DIR`. It must be one literal path whose
filename is exactly `NAME.hpp`.

For the canonical include spelling, make the output end in
`feedforge/generated/NAME.hpp`:

```cmake
feedforge_generate(
  NAME custom_events
  SCHEMA nasdaq_totalview_itch_5_0
  PIPELINE "${CMAKE_CURRENT_SOURCE_DIR}/custom_events.toml"
  OUTPUT
    "${CMAKE_CURRENT_BINARY_DIR}/artifacts/include/feedforge/generated/custom_events.hpp"
)
```

The target exposes `artifacts/include`, so the include remains
`<feedforge/generated/custom_events.hpp>`. This fixes the former behavior that
always exposed the output file's immediate parent.

For backward compatibility, any other output location ending in
`custom_events.hpp` exposes its immediate parent and is included as
`<custom_events.hpp>`. Prefer the canonical suffix in new code. A mismatched
filename, generator expression, list-valued path, path outside the active build
tree, directory, input-file alias, or duplicate generated output is rejected
during configure with the resolved path and remediation.

## Troubleshooting

- `FeedForge::compiler` is missing: install/build FeedForge with
  `FEEDFORGE_BUILD_COMPILER=ON`, or use a canonical generated target.
- The host fails the C++23 check: custom generation is unavailable on that
  toolchain; canonical targets still require only C++20.
- Sink constraints fail: provide one unambiguous typed overload per selected
  event, returning exactly `feedforge::flow` and marked `noexcept`; assert
  `sink_for_all_selected_events`.
- `FFPIPE...` or `FFSCHEMA...`: fix the reported object path in TOML.
  `FFTOML001` is syntax, `FFCLI...` is command use, and `FFIO...` is file I/O.
  Invalid input/CLI exits `2`; I/O failure exits `3`.
- CLI generation reports `FFIO002`: create the output parent directory first.
- Replay is `incomplete`: the input ended at a frame boundary without a
  zero-length end marker. Trailing bytes after the marker are a framing error.
- A custom generated header is not found: link its generated target and use the
  documented `OUTPUT` include spelling; do not add global include paths.

## Remaining integration limits

- Canonical pipelines are fixed committed projections. Different fields,
  namespace, or unknown-message policy require the C++23 host compiler.
- Targets created by `feedforge_generate()` are build-tree targets; the helper
  does not install or export a consumer's generated header.
- Source acquisition must be pinned by the consumer. FeedForge does not choose
  or update a revision for downstream projects.

## Further guidance

- [Generated C++ API](generated-api.md)
- [Schema TOML format](schema-format.md)
- [Pipeline TOML format](pipeline-format.md)
- Installed adoption skill:
  `${FeedForge_SKILL_DIR}/feedforge-adoption/SKILL.md`
- Installed pipeline-authoring skill:
  `${FeedForge_SKILL_DIR}/feedforge-pipeline-authoring/SKILL.md`
