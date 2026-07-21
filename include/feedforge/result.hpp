#ifndef FEEDFORGE_RESULT_HPP
#define FEEDFORGE_RESULT_HPP

#include <cstddef>
#include <cstdint>

#include <feedforge/config.hpp>

namespace feedforge {

enum class framing_errc : std::uint8_t;

enum class decode_status : std::uint8_t {
  emitted,
  known_unselected_skipped,
  unknown_skipped,
  stopped,
  empty_payload,
  unknown_message_type,
  invalid_message_size,
};

struct decode_outcome {
  decode_status status{};
  std::byte message_type{};
  std::uint16_t expected_size{};
  std::size_t actual_size{};

  [[nodiscard]] constexpr bool is_error() const noexcept {
    return status == decode_status::empty_payload ||
           status == decode_status::unknown_message_type ||
           status == decode_status::invalid_message_size;
  }

  [[nodiscard]] constexpr bool is_terminal() const noexcept {
    return status == decode_status::stopped || is_error();
  }
};

enum class replay_status : std::uint8_t {
  complete,
  incomplete,
  stopped,
  framing_error,
  decode_error,
};

struct replay_summary {
  replay_status status{};
  std::uint64_t frames_seen{};
  std::uint64_t events_emitted{};
  std::uint64_t known_messages_skipped{};
  std::uint64_t unknown_messages_skipped{};
  std::uint64_t bytes_consumed{};
  std::uint64_t error_offset{};
  feedforge::framing_errc framing_error{};
  feedforge::decode_outcome decode_error{};
};

}  // namespace feedforge

#endif  // FEEDFORGE_RESULT_HPP
