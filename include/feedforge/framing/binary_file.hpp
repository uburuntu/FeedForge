#pragma once

#include <array>
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
  insufficient_scratch,
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

enum class chunk_frame_status : std::uint8_t {
  frame,
  needs_input,
  complete,
  incomplete,
  error,
};

struct chunk_frame_outcome {
  chunk_frame_status status{};
  framing_errc error{};
  frame_view frame{};
  std::size_t input_consumed{};
  std::uint64_t offset{};
};

class chunked_binary_file_cursor {
public:
  constexpr explicit chunked_binary_file_cursor(std::span<std::byte> scratch) noexcept
      : scratch_(scratch) {}

  [[nodiscard]] constexpr chunk_frame_outcome next(std::span<const std::byte> input) noexcept {
    if (terminal_) {
      return terminal_outcome_;
    }

    if (saw_end_marker_) {
      if (input.empty()) {
        return make_outcome(chunk_frame_status::needs_input, framing_errc::none, 0U, committed_);
      }
      return make_terminal(chunk_frame_status::error, framing_errc::trailing_data_after_end_marker,
                           0U, committed_);
    }

    std::size_t input_position{};
    if (prefix_size_ != prefix_.size()) {
      if (prefix_size_ == 0U) {
        frame_offset_ = received_;
      }
      while (prefix_size_ != prefix_.size() && input_position != input.size()) {
        prefix_[prefix_size_] = input[input_position];
        ++prefix_size_;
        ++input_position;
        ++received_;
      }
      if (prefix_size_ != prefix_.size()) {
        return make_outcome(chunk_frame_status::needs_input, framing_errc::none, input_position,
                            frame_offset_);
      }

      const auto high = std::to_integer<std::uint16_t>(prefix_[0U]);
      const auto low = std::to_integer<std::uint16_t>(prefix_[1U]);
      payload_size_ = static_cast<std::uint16_t>((high << 8U) | low);
      payload_received_ = 0U;

      if (payload_size_ == 0U) {
        prefix_size_ = 0U;
        saw_end_marker_ = true;
        committed_ = received_;
        if (input_position != input.size()) {
          return make_terminal(chunk_frame_status::error,
                               framing_errc::trailing_data_after_end_marker, input_position,
                               committed_);
        }
        return make_outcome(chunk_frame_status::needs_input, framing_errc::none, input_position,
                            committed_);
      }

      if (static_cast<std::size_t>(payload_size_) > scratch_.size()) {
        return make_terminal(chunk_frame_status::error, framing_errc::insufficient_scratch,
                             input_position, frame_offset_);
      }
    }

    const std::size_t payload_remaining =
        static_cast<std::size_t>(payload_size_) - payload_received_;
    const std::size_t input_remaining = input.size() - input_position;
    const std::size_t copy_size =
        payload_remaining < input_remaining ? payload_remaining : input_remaining;
    for (std::size_t index = 0U; index < copy_size; ++index) {
      scratch_[payload_received_ + index] = input[input_position + index];
    }
    payload_received_ += copy_size;
    input_position += copy_size;
    received_ += copy_size;

    if (payload_received_ != static_cast<std::size_t>(payload_size_)) {
      return make_outcome(chunk_frame_status::needs_input, framing_errc::none, input_position,
                          frame_offset_);
    }

    const auto outcome = chunk_frame_outcome{
        chunk_frame_status::frame,
        framing_errc::none,
        frame_view{scratch_.first(payload_size_), to_offset(frame_offset_), ordinal_},
        input_position,
        to_offset(frame_offset_),
    };
    committed_ = received_;
    prefix_size_ = 0U;
    payload_size_ = 0U;
    payload_received_ = 0U;
    ++ordinal_;
    return outcome;
  }

  [[nodiscard]] constexpr chunk_frame_outcome finish() noexcept {
    if (terminal_) {
      return terminal_outcome_;
    }
    if (saw_end_marker_) {
      return make_terminal(chunk_frame_status::complete, framing_errc::none, 0U, committed_ - 2U);
    }
    if (prefix_size_ == 0U) {
      return make_terminal(chunk_frame_status::incomplete, framing_errc::none, 0U, received_);
    }
    if (prefix_size_ != prefix_.size()) {
      return make_terminal(chunk_frame_status::error, framing_errc::truncated_length_prefix, 0U,
                           frame_offset_);
    }
    return make_terminal(chunk_frame_status::error, framing_errc::truncated_payload, 0U,
                         frame_offset_);
  }

  [[nodiscard]] constexpr std::size_t consumed() const noexcept { return committed_; }

  [[nodiscard]] constexpr std::size_t received() const noexcept { return received_; }

private:
  [[nodiscard]] static constexpr std::uint64_t to_offset(std::size_t value) noexcept {
    return static_cast<std::uint64_t>(value);
  }

  [[nodiscard]] constexpr chunk_frame_outcome make_outcome(chunk_frame_status status,
                                                           framing_errc error,
                                                           std::size_t input_consumed,
                                                           std::size_t offset) const noexcept {
    return chunk_frame_outcome{status, error, frame_view{}, input_consumed, to_offset(offset)};
  }

  [[nodiscard]] constexpr chunk_frame_outcome make_terminal(chunk_frame_status status,
                                                            framing_errc error,
                                                            std::size_t input_consumed,
                                                            std::size_t offset) noexcept {
    terminal_outcome_ = make_outcome(status, error, input_consumed, offset);
    terminal_ = true;
    return terminal_outcome_;
  }

  std::span<std::byte> scratch_;
  std::array<std::byte, 2U> prefix_{};
  std::size_t prefix_size_{};
  std::uint16_t payload_size_{};
  std::size_t payload_received_{};
  std::size_t frame_offset_{};
  std::size_t received_{};
  std::size_t committed_{};
  std::uint64_t ordinal_{};
  chunk_frame_outcome terminal_outcome_{};
  bool saw_end_marker_{};
  bool terminal_{};
};

}  // namespace feedforge
