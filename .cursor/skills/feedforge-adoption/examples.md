# FeedForge adoption examples

## Canonical order-events consumer

`CMakeLists.txt`:

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

`main.cpp`:

```cpp
#include <feedforge/generated/nasdaq/itch50_order_events.hpp>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <span>
#include <vector>

namespace events =
    feedforge::generated::nasdaq::itch50_order_events;

struct application_sink {
  std::uint64_t events_seen{};

  feedforge::flow operator()(events::add_order const& event) noexcept {
    last_order_reference = event.order_reference_number.value;
    return accepted();
  }

  feedforge::flow operator()(events::add_order_mpid const&) noexcept {
    return accepted();
  }

  feedforge::flow operator()(events::order_executed const&) noexcept {
    return accepted();
  }

  feedforge::flow operator()(
      events::order_executed_with_price const&) noexcept {
    return accepted();
  }

  feedforge::flow operator()(events::order_cancel const&) noexcept {
    return accepted();
  }

  feedforge::flow operator()(events::order_delete const&) noexcept {
    return accepted();
  }

  feedforge::flow operator()(events::order_replace const&) noexcept {
    return accepted();
  }

  feedforge::flow operator()(events::trade const&) noexcept {
    return accepted();
  }

  std::uint64_t last_order_reference{};

 private:
  feedforge::flow accepted() noexcept {
    ++events_seen;
    return feedforge::flow::continue_;
  }
};

static_assert(
    events::sink_for_all_selected_events<application_sink>);

int main(int argc, char** argv) {
  if (argc != 2) {
    return 2;
  }

  std::ifstream input{argv[1], std::ios::binary | std::ios::ate};
  if (!input) {
    return 3;
  }

  const std::streamoff end = input.tellg();
  if (end < 0 ||
      static_cast<std::uintmax_t>(end) >
          static_cast<std::uintmax_t>(
              std::numeric_limits<std::size_t>::max()) ||
      static_cast<std::uintmax_t>(end) >
          static_cast<std::uintmax_t>(
              std::numeric_limits<std::streamsize>::max())) {
    return 3;
  }

  std::vector<std::byte> bytes(static_cast<std::size_t>(end));
  input.seekg(0, std::ios::beg);
  if (!bytes.empty()) {
    input.read(reinterpret_cast<char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  }
  if (!input) {
    return 3;
  }

  application_sink sink;
  const feedforge::replay_summary summary =
      events::replay_binary_file(
          std::span<const std::byte>{bytes.data(), bytes.size()}, sink);

  if (sink.events_seen != summary.events_emitted) {
    return 4;
  }

  switch (summary.status) {
    case feedforge::replay_status::complete:
      return 0;
    case feedforge::replay_status::stopped:
      return 0;  // Replace with the application's cooperative-stop result.
    case feedforge::replay_status::incomplete:
      return 5;
    case feedforge::replay_status::framing_error:
      // Log summary.framing_error and summary.error_offset outside replay.
      return 6;
    case feedforge::replay_status::decode_error:
      // Log summary.decode_error fields and summary.error_offset.
      return 7;
  }
  return 8;
}
```

Configure, build, and replay a valid empty complete session:

```sh
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH=/absolute/feedforge-prefix
cmake --build build
printf '\000\000' > build/empty-complete.binaryfile
build/market_data_consumer build/empty-complete.binaryfile
```

The file allocation and I/O happen before `replay_binary_file()`. The input
vector stays alive through replay. Each event reference is consumed only during
its typed overload.

## Direct decode with explicit outcome handling

```cpp
application_sink sink;
events::decoder decoder;
const feedforge::decode_outcome outcome =
    decoder.decode_one(payload, sink);

if (outcome.is_error()) {
  // Inspect status, message_type, expected_size, and actual_size.
  return decode_failed;
}
if (outcome.status == feedforge::decode_status::stopped) {
  return stopped_by_sink;
}
if (outcome.status ==
        feedforge::decode_status::known_unselected_skipped ||
    outcome.status == feedforge::decode_status::unknown_skipped) {
  return skipped_successfully;
}
return emitted_successfully;
```

Do not require `emitted` when using a projection pipeline: a valid known message
outside the projection intentionally returns `known_unselected_skipped`.
