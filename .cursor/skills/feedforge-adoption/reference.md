# FeedForge consumer reference

## Public package surface

| Use case | CMake target | Header | C++ namespace |
|---|---|---|---|
| Eight order events | `FeedForge::generated::itch50_order_events` | `<feedforge/generated/nasdaq/itch50_order_events.hpp>` | `feedforge::generated::nasdaq::itch50_order_events` |
| All 23 schema messages | `FeedForge::generated::itch50_all` | `<feedforge/generated/nasdaq/itch50_all.hpp>` | `feedforge::generated::nasdaq::itch50_all` |
| Runtime primitives only | `FeedForge::runtime` | Public headers under `<feedforge/...>` | `feedforge` |
| Host compiler | `FeedForge::compiler` | Executable target; no consumer header | n/a |

The runtime and generated code require C++20. Building `FeedForge::compiler` requires C++23. Canonical generated targets do not require the compiler or toml++.

The order-events target selects:

`add_order`, `add_order_mpid`, `order_executed`,
`order_executed_with_price`, `order_cancel`, `order_delete`,
`order_replace`, and `trade`.

## Config-package patterns

### Canonical target

```cmake
cmake_minimum_required(VERSION 3.25)
project(MarketDataConsumer LANGUAGES CXX)

find_package(FeedForge CONFIG REQUIRED)

add_executable(market_data_consumer main.cpp)
target_compile_features(market_data_consumer PRIVATE cxx_std_20)
set_target_properties(market_data_consumer PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(
  market_data_consumer
  PRIVATE FeedForge::generated::itch50_order_events
)
```

The package producer may set `FEEDFORGE_BUILD_COMPILER=OFF`; both canonical generated targets are still installed.

### Installed custom generation

```cmake
find_package(FeedForge CONFIG REQUIRED)

if(NOT TARGET FeedForge::compiler)
  message(FATAL_ERROR
    "This pipeline needs a compiler-enabled FeedForge package")
endif()

feedforge_generate(
  NAME my_orders
  SCHEMA nasdaq_totalview_itch_5_0
  PIPELINE "${CMAKE_CURRENT_SOURCE_DIR}/my_orders.toml"
)

add_executable(market_data_consumer main.cpp)
target_compile_features(market_data_consumer PRIVATE cxx_std_20)
target_link_libraries(
  market_data_consumer
  PRIVATE FeedForge::generated::my_orders
)
```

With the default output:

- `NAME my_orders` creates `FeedForge::generated::my_orders`;
- the generated include is `<feedforge/generated/my_orders.hpp>`;
- the C++ namespace comes from `namespace` in `my_orders.toml`, not from `NAME`;
- `SCHEMA nasdaq_totalview_itch_5_0` resolves the installed canonical schema;
- a relative `PIPELINE` is resolved from the consumer source directory.

Use a lowercase snake-case `NAME` matching the pipeline name and namespace leaf unless the project has a reason not to. `NAME` must be a C++ identifier. The helper rejects duplicate targets, missing inputs, and outputs outside the build tree.

## FetchContent patterns

Pin `GIT_TAG` to an immutable revision reviewed by the consuming project.

### Canonical, compiler disabled

```cmake
include(FetchContent)

FetchContent_Declare(
  feedforge
  GIT_REPOSITORY https://github.com/uburuntu/FeedForge.git
  GIT_TAG <immutable-reviewed-revision>
  GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(feedforge)

target_link_libraries(
  market_data_consumer
  PRIVATE FeedForge::generated::itch50_order_events
)
```

Compiler, tests, examples, fuzzers, and benchmarks are disabled by default when
FeedForge is a dependency. Do not turn on the compiler for a canonical
consumer.

### Custom generation

Use the same pattern with `FEEDFORGE_BUILD_COMPILER ON`, then call `feedforge_generate()` as shown above. The configure/build host must support C++23. FeedForge first looks for exactly tomlplusplus 3.4.0 as a config package and otherwise fetches its pinned source for the compiler build; it is not linked into the generated consumer target.

## Sink and lifetime contract

For namespace alias `events`, each selected event requires:

```cpp
feedforge::flow operator()(events::event_name const&) noexcept;
```

The generated `events::sink_for_all_selected_events<Sink>` concept rejects:

- a missing or ambiguous overload;
- a potentially throwing overload; or
- a return type other than exactly `feedforge::flow`.

`feedforge::flow::continue_` continues replay. `feedforge::flow::stop` delivers the current event, counts it as emitted, and then returns a non-error stopped result.

The payload span lives through `decode_one()`. The BinaryFILE span lives through
`replay_binary_file()`. A chunk passed to `chunked_replayer::push()` lives
through that call; the scratch span and sink live through the adapter. A
generated event owns all projected field values, but the `const&` passed to the
sink lives only for that call. Copy the event, not its reference, if it must
outlive the callback.

## Value representations

| Schema logical type | C++ representation | Access |
|---|---|---|
| `stock_locate`, `tracking_number`, `order_reference_number`, `match_number`, `share_count` | Strong `feedforge` wrapper | `.value` |
| `timestamp_ns` | `feedforge::timestamp_ns` | `.value` |
| Decimal | `feedforge::decimal<Rep, Scale>` | `.raw`, static `.scale` |
| ASCII or alpha | `feedforge::ascii<N>` | `.raw`, `.trimmed()` |
| Raw unsigned | Fixed-width unsigned integer | value directly |

Do not convert a decimal to floating point implicitly or treat a timestamp as a wall-clock object. Preserve the generated type at module boundaries unless the application defines and tests an explicit conversion.

## Decode outcomes

`decode_one()` returns `feedforge::decode_outcome`.

| Status | Meaning |
|---|---|
| `emitted` | Selected valid event delivered; sink continued |
| `known_unselected_skipped` | Known valid message omitted by the pipeline |
| `unknown_skipped` | Unknown discriminator accepted by `"skip"` policy |
| `stopped` | Event delivered; sink requested stop |
| `empty_payload` | No discriminator byte; error |
| `unknown_message_type` | Unknown discriminator under `"error"` policy; error |
| `invalid_message_size` | Known message size differs from schema; error |

`is_error()` is true for the final three error statuses. `is_terminal()` is true for those errors and `stopped`.

## BinaryFILE replay outcomes

`replay_binary_file()` returns `feedforge::replay_summary`.

| Status | Required handling |
|---|---|
| `complete` | End marker consumed; no trailing byte |
| `incomplete` | Input ended at a frame boundary without an end marker |
| `stopped` | Sink requested stop; not an error |
| `framing_error` | Inspect `framing_error` and `error_offset` |
| `decode_error` | Inspect `decode_error` and `error_offset` |

Strict replay rejects a one-byte length prefix, truncated payload, and any byte after an end marker. It stops at the first error or sink stop.

For arbitrary chunk boundaries, construct
`events::chunked_replayer<Sink>{scratch, sink}`, call `push(chunk)` in order,
and call `finish()` at end of input. A zero marker remains provisional until
finish so later trailing data is still rejected. Scratch smaller than a
declared payload produces `framing_errc::insufficient_scratch`; 65,535 bytes
accepts the complete BinaryFILE length domain. With sufficient scratch, the
terminal summary and delivered event sequence match one-shot replay.

## Install/build/run checklist

Create a runtime-only package:

```sh
cmake -S /path/to/FeedForge -B build/feedforge-package \
  -DCMAKE_BUILD_TYPE=Release \
  -DFEEDFORGE_BUILD_COMPILER=OFF \
  -DFEEDFORGE_BUILD_TESTS=OFF \
  -DFEEDFORGE_BUILD_EXAMPLES=OFF
cmake --build build/feedforge-package --config Release
cmake --install build/feedforge-package --config Release \
  --prefix /absolute/feedforge-prefix
```

Configure an external canonical consumer:

```sh
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH=/absolute/feedforge-prefix
cmake --build build --target market_data_consumer
printf '\000\000' > build/empty-complete.binaryfile
build/market_data_consumer build/empty-complete.binaryfile
```

For custom generation, install FeedForge with `FEEDFORGE_BUILD_COMPILER=ON`, validate the pipeline before configuring the consumer, and confirm the package contains the imported `FeedForge::compiler` target.

If package export behavior changes, inspect these properties:

```cmake
get_target_property(
  _compile_options
  FeedForge::generated::itch50_order_events
  INTERFACE_COMPILE_OPTIONS
)
get_target_property(
  _link_options
  FeedForge::generated::itch50_order_events
  INTERFACE_LINK_OPTIONS
)
get_target_property(
  _links
  FeedForge::generated::itch50_order_events
  INTERFACE_LINK_LIBRARIES
)
```

The compile and link options should be empty/not found. The canonical generated target should link only `FeedForge::runtime`; it should not expose toml++, project warning flags, or sanitizer flags.

## Custom `OUTPUT`

The default layout is `<feedforge/generated/NAME.hpp>`. An explicit `OUTPUT` is
resolved from the current binary directory, must remain in the top-level build
tree, and must be a literal path named exactly `NAME.hpp`.

When the path ends in `feedforge/generated/NAME.hpp`, the helper derives and
exports the include root before `feedforge`, preserving the default include
spelling. Other matching locations retain the legacy flat `<NAME.hpp>` include
from the output's parent. Mismatched filenames, unsafe paths, input aliases, and
duplicate outputs fail at configure time.
