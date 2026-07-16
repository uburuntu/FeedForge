#include "test_support.hpp"

#include <feedforge/generated/nasdaq/itch50_all.hpp>
#include <feedforge/generated/nasdaq/itch50_order_events.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>
#include <span>

#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace allocation_probe {

std::atomic_size_t allocations{};
std::atomic_bool measuring{};

void record_allocation() noexcept {
  if (measuring.load(std::memory_order_relaxed)) {
    static_cast<void>(allocations.fetch_add(1U, std::memory_order_relaxed));
  }
}

[[nodiscard]] void* unaligned_allocate(const std::size_t size) noexcept {
  return std::malloc(size == 0U ? 1U : size);
}

[[nodiscard]] void* aligned_allocate(const std::size_t size, const std::size_t alignment) noexcept {
#if defined(_MSC_VER)
  return _aligned_malloc(size == 0U ? 1U : size, alignment);
#else
  const std::size_t requested = size == 0U ? 1U : size;
  if (requested > std::numeric_limits<std::size_t>::max() - (alignment - 1U)) {
    return nullptr;
  }
  const std::size_t rounded = ((requested + alignment - 1U) / alignment) * alignment;
  return std::aligned_alloc(alignment, rounded);
#endif
}

void aligned_deallocate(void* const pointer) noexcept {
#if defined(_MSC_VER)
  _aligned_free(pointer);
#else
  std::free(pointer);
#endif
}

[[noreturn]] void allocation_failed() { throw std::bad_alloc{}; }

void begin() noexcept {
  allocations.store(0U, std::memory_order_relaxed);
  measuring.store(true, std::memory_order_release);
}

[[nodiscard]] std::size_t end() noexcept {
  measuring.store(false, std::memory_order_release);
  return allocations.load(std::memory_order_relaxed);
}

} // namespace allocation_probe

void* operator new(const std::size_t size) {
  allocation_probe::record_allocation();
  if (void* const pointer = allocation_probe::unaligned_allocate(size)) {
    return pointer;
  }
  allocation_probe::allocation_failed();
}

void* operator new[](const std::size_t size) { return ::operator new(size); }

void* operator new(const std::size_t size, const std::nothrow_t&) noexcept {
  allocation_probe::record_allocation();
  return allocation_probe::unaligned_allocate(size);
}

void* operator new[](const std::size_t size, const std::nothrow_t& tag) noexcept {
  return ::operator new(size, tag);
}

void* operator new(const std::size_t size, const std::align_val_t alignment) {
  allocation_probe::record_allocation();
  if (void* const pointer =
          allocation_probe::aligned_allocate(size, static_cast<std::size_t>(alignment))) {
    return pointer;
  }
  allocation_probe::allocation_failed();
}

void* operator new[](const std::size_t size, const std::align_val_t alignment) {
  return ::operator new(size, alignment);
}

void* operator new(const std::size_t size, const std::align_val_t alignment,
                   const std::nothrow_t&) noexcept {
  allocation_probe::record_allocation();
  return allocation_probe::aligned_allocate(size, static_cast<std::size_t>(alignment));
}

void* operator new[](const std::size_t size, const std::align_val_t alignment,
                     const std::nothrow_t& tag) noexcept {
  return ::operator new(size, alignment, tag);
}

void operator delete(void* const pointer) noexcept { std::free(pointer); }

void operator delete[](void* const pointer) noexcept { ::operator delete(pointer); }

void operator delete(void* const pointer, std::size_t) noexcept { ::operator delete(pointer); }

void operator delete[](void* const pointer, std::size_t) noexcept { ::operator delete(pointer); }

void operator delete(void* const pointer, const std::nothrow_t&) noexcept {
  ::operator delete(pointer);
}

void operator delete[](void* const pointer, const std::nothrow_t&) noexcept {
  ::operator delete(pointer);
}

void operator delete(void* const pointer, const std::align_val_t) noexcept {
  allocation_probe::aligned_deallocate(pointer);
}

void operator delete[](void* const pointer, const std::align_val_t alignment) noexcept {
  ::operator delete(pointer, alignment);
}

void operator delete(void* const pointer, std::size_t, const std::align_val_t alignment) noexcept {
  ::operator delete(pointer, alignment);
}

void operator delete[](void* const pointer, std::size_t,
                       const std::align_val_t alignment) noexcept {
  ::operator delete(pointer, alignment);
}

void operator delete(void* const pointer, const std::align_val_t alignment,
                     const std::nothrow_t&) noexcept {
  ::operator delete(pointer, alignment);
}

void operator delete[](void* const pointer, const std::align_val_t alignment,
                       const std::nothrow_t&) noexcept {
  ::operator delete(pointer, alignment);
}

namespace {

namespace all_messages = feedforge::generated::nasdaq::itch50_all;
namespace order_events = feedforge::generated::nasdaq::itch50_order_events;

template <std::size_t Size>
[[nodiscard]] constexpr std::array<std::byte, Size + 2U>
frame(const std::array<std::byte, Size>& payload) noexcept {
  std::array<std::byte, Size + 2U> result{};
  result[0U] = std::byte{static_cast<unsigned char>((Size >> 8U) & 0xffU)};
  result[1U] = std::byte{static_cast<unsigned char>(Size & 0xffU)};
  for (std::size_t index = 0U; index < Size; ++index) {
    result[index + 2U] = payload[index];
  }
  return result;
}

template <std::size_t Size>
[[nodiscard]] constexpr std::array<std::byte, Size + 2U>
with_end_marker(const std::array<std::byte, Size>& input) noexcept {
  std::array<std::byte, Size + 2U> result{};
  for (std::size_t index = 0U; index < Size; ++index) {
    result[index] = input[index];
  }
  return result;
}

constexpr auto add_order_payload = [] {
  std::array<std::byte, 36U> payload{};
  payload[0U] = std::byte{'A'};
  return payload;
}();

constexpr auto system_event_payload = [] {
  std::array<std::byte, 12U> payload{};
  payload[0U] = std::byte{'S'};
  return payload;
}();

constexpr auto add_order_frame = frame(add_order_payload);
constexpr auto add_order_complete = with_end_marker(add_order_frame);
constexpr auto system_event_complete = with_end_marker(frame(system_event_payload));

struct noop_sink {
  std::size_t calls{};
  bool stop{};

  template <class Event> feedforge::flow operator()(const Event&) noexcept {
    ++calls;
    return stop ? feedforge::flow::stop : feedforge::flow::continue_;
  }
};

static_assert(all_messages::sink_for_all_selected_events<noop_sink>);
static_assert(order_events::sink_for_all_selected_events<noop_sink>);

void check_decode_paths() {
  all_messages::decoder all_decoder;
  order_events::decoder projected_decoder;

  noop_sink emitted_sink;
  allocation_probe::begin();
  const auto emitted = all_decoder.decode_one(add_order_payload, emitted_sink);
  const std::size_t emitted_allocations = allocation_probe::end();
  FEEDFORGE_CHECK(emitted_allocations == 0U);
  FEEDFORGE_CHECK(emitted.status == feedforge::decode_status::emitted);
  FEEDFORGE_CHECK(emitted_sink.calls == 1U);

  noop_sink skipped_sink;
  allocation_probe::begin();
  const auto skipped = projected_decoder.decode_one(system_event_payload, skipped_sink);
  const std::size_t skipped_allocations = allocation_probe::end();
  FEEDFORGE_CHECK(skipped_allocations == 0U);
  FEEDFORGE_CHECK(skipped.status == feedforge::decode_status::known_unselected_skipped);
  FEEDFORGE_CHECK(skipped_sink.calls == 0U);

  noop_sink stopped_sink{0U, true};
  allocation_probe::begin();
  const auto stopped = projected_decoder.decode_one(add_order_payload, stopped_sink);
  const std::size_t stopped_allocations = allocation_probe::end();
  FEEDFORGE_CHECK(stopped_allocations == 0U);
  FEEDFORGE_CHECK(stopped.status == feedforge::decode_status::stopped);
  FEEDFORGE_CHECK(stopped_sink.calls == 1U);
}

void check_replay_paths() {
  noop_sink complete_sink;
  allocation_probe::begin();
  const auto complete = order_events::replay_binary_file(add_order_complete, complete_sink);
  const std::size_t complete_allocations = allocation_probe::end();
  FEEDFORGE_CHECK(complete_allocations == 0U);
  FEEDFORGE_CHECK(complete.status == feedforge::replay_status::complete);
  FEEDFORGE_CHECK(complete.frames_seen == 1U && complete.events_emitted == 1U);

  noop_sink incomplete_sink;
  allocation_probe::begin();
  const auto incomplete = order_events::replay_binary_file(add_order_frame, incomplete_sink);
  const std::size_t incomplete_allocations = allocation_probe::end();
  FEEDFORGE_CHECK(incomplete_allocations == 0U);
  FEEDFORGE_CHECK(incomplete.status == feedforge::replay_status::incomplete);
  FEEDFORGE_CHECK(incomplete.frames_seen == 1U && incomplete.events_emitted == 1U);

  noop_sink skipped_sink;
  allocation_probe::begin();
  const auto skipped = order_events::replay_binary_file(system_event_complete, skipped_sink);
  const std::size_t skipped_allocations = allocation_probe::end();
  FEEDFORGE_CHECK(skipped_allocations == 0U);
  FEEDFORGE_CHECK(skipped.status == feedforge::replay_status::complete);
  FEEDFORGE_CHECK(skipped.known_messages_skipped == 1U && skipped_sink.calls == 0U);

  noop_sink stopped_sink{0U, true};
  allocation_probe::begin();
  const auto stopped = order_events::replay_binary_file(add_order_frame, stopped_sink);
  const std::size_t stopped_allocations = allocation_probe::end();
  FEEDFORGE_CHECK(stopped_allocations == 0U);
  FEEDFORGE_CHECK(stopped.status == feedforge::replay_status::stopped);
  FEEDFORGE_CHECK(stopped.frames_seen == 1U && stopped.events_emitted == 1U);
  FEEDFORGE_CHECK(stopped_sink.calls == 1U);
}

} // namespace

int main() {
  check_decode_paths();
  check_replay_paths();
}
