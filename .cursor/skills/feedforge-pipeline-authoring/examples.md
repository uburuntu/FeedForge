# FeedForge pipeline examples

## Narrow order lifecycle pipeline

`config/market_events.toml`:

```toml
format_version = 1
name = "market_events"
namespace = "acme::feed::market_events"
schema = "nasdaq_totalview_itch_5_0"
profile = "portable_checked"
unknown_messages = "error"
unselected_messages = "skip"

[[emit]]
source = "A"
event = "add_order"
fields = [
  "stock_locate",
  "timestamp",
  "order_reference_number",
  "buy_sell_indicator",
  "shares",
  "stock",
  "price",
]

[[emit]]
source = "X"
event = "order_cancel"
fields = [
  "stock_locate",
  "timestamp",
  "order_reference_number",
  "cancelled_shares",
]
```

The explicit order becomes C++ aggregate member order. `order_reference_number`,
`shares`, timestamps, prices, and ASCII fields retain their generated strong
types.

## Validate and compile from the source tree

Configure the compiler once:

```sh
cmake -S . -B build/feedforge-tools \
  -DFEEDFORGE_BUILD_COMPILER=ON \
  -DFEEDFORGE_BUILD_TESTS=OFF \
  -DFEEDFORGE_BUILD_EXAMPLES=OFF
cmake --build build/feedforge-tools --target feedforgec
```

Validate schema and pipeline:

```sh
build/feedforge-tools/src/feedforgec/feedforgec validate \
  --schema schemas/nasdaq/totalview_itch_5_0.toml

build/feedforge-tools/src/feedforgec/feedforgec validate \
  --schema schemas/nasdaq/totalview_itch_5_0.toml \
  --pipeline config/market_events.toml
```

Generate inspectable output:

```sh
cmake -E make_directory build/generated
build/feedforge-tools/src/feedforgec/feedforgec compile \
  --schema schemas/nasdaq/totalview_itch_5_0.toml \
  --pipeline config/market_events.toml \
  --output build/generated/market_events.hpp \
  --dump-ir build/generated/market_events.ffir.json
```

The CLI output path is suitable for inspection and determinism checks. Use the
CMake helper below for a consumer target so dependency ordering and include
paths are attached to that target.

## Generate from an external CMake consumer

```cmake
cmake_minimum_required(VERSION 3.25)
project(MarketEventsConsumer LANGUAGES CXX)

find_package(FeedForge CONFIG REQUIRED)

if(NOT TARGET FeedForge::compiler)
  message(FATAL_ERROR
    "market_events requires a compiler-enabled FeedForge package")
endif()

feedforge_generate(
  NAME market_events
  SCHEMA nasdaq_totalview_itch_5_0
  PIPELINE "${CMAKE_CURRENT_SOURCE_DIR}/config/market_events.toml"
)

add_executable(market_events_consumer main.cpp)
target_compile_features(market_events_consumer PRIVATE cxx_std_20)
set_target_properties(
  market_events_consumer
  PROPERTIES CXX_EXTENSIONS OFF
)
target_link_libraries(
  market_events_consumer
  PRIVATE FeedForge::generated::market_events
)
```

`main.cpp`:

```cpp
#include <feedforge/generated/market_events.hpp>

#include <cstdint>

namespace events = acme::feed::market_events;

struct sink {
  feedforge::flow operator()(events::add_order const& event) noexcept {
    last_reference = event.order_reference_number.value;
    last_price_raw = event.price.raw;
    return feedforge::flow::continue_;
  }

  feedforge::flow operator()(events::order_cancel const& event) noexcept {
    last_reference = event.order_reference_number.value;
    return feedforge::flow::continue_;
  }

  std::uint64_t last_reference{};
  std::uint32_t last_price_raw{};
};

static_assert(events::sink_for_all_selected_events<sink>);
```

The helper creates `<feedforge/generated/market_events.hpp>` under the build
tree. The include filename comes from `NAME`; `acme::feed::market_events` comes
from pipeline TOML.

## Determinism check

```sh
cmake -E make_directory build/repeat/a build/repeat/b

build/feedforge-tools/src/feedforgec/feedforgec compile \
  --schema schemas/nasdaq/totalview_itch_5_0.toml \
  --pipeline config/market_events.toml \
  --output build/repeat/a/market_events.hpp

build/feedforge-tools/src/feedforgec/feedforgec compile \
  --schema schemas/nasdaq/totalview_itch_5_0.toml \
  --pipeline config/market_events.toml \
  --output build/repeat/b/market_events.hpp

cmake -E compare_files \
  build/repeat/a/market_events.hpp \
  build/repeat/b/market_events.hpp
```

Any nonzero comparison is a defect when inputs are semantically identical.

## Unsupported request example

Do not encode this request in TOML:

> Emit `notional = shares * price`, rename `stock` to `symbol`, and ignore
> malformed known messages.

Format version 1 has no computed fields, renames, or policy that disables exact
size validation. Project `shares`, `price`, and `stock` with their schema names;
compute a domain value in the typed sink. Malformed known messages remain decode
errors.
