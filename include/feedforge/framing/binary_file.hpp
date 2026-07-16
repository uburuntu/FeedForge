#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include <feedforge/config.hpp>

namespace feedforge {

enum class frame_status : std::uint8_t {
  frame,
  complete,
  incomplete,
  error,
};

enum class framing_errc : std::uint8_t {
  none,
  truncated_length_prefix,
  truncated_payload,
  trailing_data_after_end_marker,
};

struct frame_view {
  std::span<const std::byte> payload;
  std::uint64_t file_offset{};
  std::uint64_t ordinal{};
};

struct frame_outcome {
  frame_status status{};
  framing_errc error{};
  frame_view frame{};
  std::uint64_t offset{};
};

class binary_file_cursor {
 public:
  constexpr explicit binary_file_cursor(
      std::span<const std::byte> input) noexcept
      : input_(input) {}

  [[nodiscard]] constexpr frame_outcome next() noexcept {
    if (terminal_) {
      return terminal_outcome_;
    }

    const auto prefix_offset = position_;
    const auto available = input_.size() - position_;

    if (available == 0U) {
      return make_terminal(frame_status::incomplete, framing_errc::none,
                           input_.size());
    }

    if (available == 1U) {
      return make_terminal(frame_status::error,
                           framing_errc::truncated_length_prefix,
                           prefix_offset);
    }

    const auto high = std::to_integer<std::uint16_t>(input_[position_]);
    const auto low = std::to_integer<std::uint16_t>(input_[position_ + 1U]);
    const auto payload_size =
        static_cast<std::uint16_t>((high << 8U) | low);

    if (payload_size == 0U) {
      position_ += 2U;
      return make_terminal(frame_status::complete, framing_errc::none,
                           prefix_offset);
    }

    if (static_cast<std::size_t>(payload_size) > available - 2U) {
      return make_terminal(frame_status::error,
                           framing_errc::truncated_payload, prefix_offset);
    }

    const auto payload = input_.subspan(position_ + 2U, payload_size);
    const auto outcome = frame_outcome{
        frame_status::frame,
        framing_errc::none,
        frame_view{payload, to_offset(prefix_offset), ordinal_},
        to_offset(prefix_offset),
    };

    position_ += 2U + static_cast<std::size_t>(payload_size);
    ++ordinal_;
    return outcome;
  }

  [[nodiscard]] constexpr std::size_t consumed() const noexcept {
    return position_;
  }

  [[nodiscard]] constexpr std::span<const std::byte> remaining() const noexcept {
    if (terminal_ && terminal_outcome_.status == frame_status::complete) {
      return input_.subspan(position_);
    }
    return {};
  }

 private:
  [[nodiscard]] static constexpr std::uint64_t to_offset(
      std::size_t value) noexcept {
    return static_cast<std::uint64_t>(value);
  }

  [[nodiscard]] constexpr frame_outcome make_terminal(
      frame_status status, framing_errc error, std::size_t offset) noexcept {
    terminal_outcome_ =
        frame_outcome{status, error, frame_view{}, to_offset(offset)};
    terminal_ = true;
    return terminal_outcome_;
  }

  std::span<const std::byte> input_;
  std::size_t position_{};
  std::uint64_t ordinal_{};
  frame_outcome terminal_outcome_{};
  bool terminal_{};
};

}  // namespace feedforge
