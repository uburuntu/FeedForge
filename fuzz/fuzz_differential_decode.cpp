#include "../tests/reference/itch50_differential.hpp"

#include "fuzz_support.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {

namespace reference = feedforge_reference::itch50;

void require_match(const std::span<const std::byte> payload) noexcept {
  feedforge::fuzz::require(reference::matches_generated(payload));
}

void exercise_forced_valid_messages(const std::uint8_t* const data,
                                    const std::size_t size) noexcept {
  std::array<std::byte, reference::maximum_message_size + 1U> storage{};

  for (std::size_t layout_index = 0U; layout_index < reference::message_layouts.size();
       ++layout_index) {
    const reference::message_layout& layout = reference::message_layouts[layout_index];
    std::uint32_t state = 0x85ebca6bU ^ static_cast<std::uint32_t>(layout_index * 0x9e37U);
    for (std::size_t index = 0U; index <= layout.size; ++index) {
      state = state * 1664525U + 1013904223U;
      const unsigned int source =
          size == 0U ? 0U : static_cast<unsigned int>(data[(index + layout_index) % size]);
      const unsigned int mixed = source ^ (state >> 24U);
      storage[index] = std::byte{static_cast<unsigned char>(mixed & 0xffU)};
    }
    storage.front() = layout.discriminator;

    const auto valid = std::span<const std::byte>{storage.data(), layout.size};
    require_match(valid);
    require_match(valid.first(layout.size - 1U));
    require_match(std::span<const std::byte>{storage.data(), layout.size + 1U});
  }

  storage.front() = std::byte{0U};
  require_match(std::span<const std::byte>{storage.data(), storage.size()});
}

} // namespace

// Callable root for CTest standalone smoke and libFuzzer integration.
int feedforge_fuzz_differential_decode_input(const std::uint8_t* const data,
                                             const std::size_t size) noexcept {
  const auto direct = std::span<const std::byte>{reinterpret_cast<const std::byte*>(data), size};
  require_match(direct);
  exercise_forced_valid_messages(data, size);
  return 0;
}

#if defined(FEEDFORGE_FUZZ_STANDALONE)
int main() {
  return feedforge::fuzz::run_standalone_smoke(feedforge_fuzz_differential_decode_input);
}
#else
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* const data,
                                      const std::size_t size) noexcept {
  return feedforge_fuzz_differential_decode_input(data, size);
}
#endif
