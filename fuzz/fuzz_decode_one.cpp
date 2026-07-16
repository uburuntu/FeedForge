#include <feedforge/generated/nasdaq/itch50_all.hpp>

#include "fuzz_support.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace {

namespace generated = feedforge::generated::nasdaq::itch50_all;

struct counting_implementation {
  static constexpr std::string_view variant_id{"fuzz_counting.v1"};
  static inline std::size_t loads{};

  template <std::size_t Width>
    requires feedforge::wire::supported_unsigned_width<Width>
  [[nodiscard]] static std::uint64_t load_unsigned(const std::byte* const first) noexcept {
    ++loads;
    return feedforge::profile::portable_checked::load_unsigned<Width>(first);
  }
};

static_assert(feedforge::decoder_implementation<counting_implementation>);

struct observing_noop_sink {
  std::size_t calls{};
  std::byte last_type{};
  bool stop{};

  template <class Event> feedforge::flow operator()(const Event&) noexcept {
    ++calls;
    last_type = Event::source_discriminator;
    return stop ? feedforge::flow::stop : feedforge::flow::continue_;
  }
};

static_assert(generated::sink_for_all_selected_events<observing_noop_sink>);

struct observation {
  feedforge::decode_outcome outcome{};
  std::size_t sink_calls{};
  std::size_t loads{};
  std::byte last_type{};
};

[[nodiscard]] bool same_outcome(const feedforge::decode_outcome& lhs,
                                const feedforge::decode_outcome& rhs) noexcept {
  return lhs.status == rhs.status && lhs.message_type == rhs.message_type &&
         lhs.expected_size == rhs.expected_size && lhs.actual_size == rhs.actual_size;
}

[[nodiscard]] std::uint16_t expected_size_for(const std::byte type) noexcept {
  for (const auto known : generated::pipeline_metadata::known_messages) {
    if (known.discriminator == type) {
      return known.size;
    }
  }
  return 0U;
}

[[nodiscard]] observation decode(const std::span<const std::byte> payload,
                                 const bool stop) noexcept {
  generated::basic_decoder<counting_implementation> decoder;
  observing_noop_sink sink{0U, std::byte{0U}, stop};
  counting_implementation::loads = 0U;
  const feedforge::decode_outcome outcome = decoder.decode_one(payload, sink);
  return observation{outcome, sink.calls, counting_implementation::loads, sink.last_type};
}

void validate(const std::span<const std::byte> payload, const observation& result,
              const bool stop) noexcept {
  using feedforge::fuzz::require;

  require(result.outcome.actual_size == payload.size());
  require(result.sink_calls <= 1U);
  if (payload.empty()) {
    require(result.outcome.status == feedforge::decode_status::empty_payload);
    require(result.outcome.message_type == std::byte{0U});
    require(result.outcome.expected_size == 0U);
    require(result.outcome.is_error() && result.outcome.is_terminal());
    require(result.sink_calls == 0U && result.loads == 0U);
    return;
  }

  const std::byte type = payload.front();
  const std::uint16_t expected_size = expected_size_for(type);
  require(result.outcome.message_type == type);
  require(result.outcome.expected_size == expected_size);

  if (expected_size == 0U) {
    require(result.outcome.status == feedforge::decode_status::unknown_message_type);
    require(result.outcome.is_error() && result.outcome.is_terminal());
    require(result.sink_calls == 0U && result.loads == 0U);
    return;
  }

  if (payload.size() != static_cast<std::size_t>(expected_size)) {
    require(result.outcome.status == feedforge::decode_status::invalid_message_size);
    require(result.outcome.is_error() && result.outcome.is_terminal());
    require(result.sink_calls == 0U && result.loads == 0U);
    return;
  }

  require(result.outcome.status ==
          (stop ? feedforge::decode_status::stopped : feedforge::decode_status::emitted));
  require(!result.outcome.is_error());
  require(result.outcome.is_terminal() == stop);
  require(result.sink_calls == 1U);
  require(result.last_type == type);
}

void exercise_decode(const std::span<const std::byte> payload) noexcept {
  using feedforge::fuzz::require;

  const observation first = decode(payload, false);
  const observation second = decode(payload, false);
  validate(payload, first, false);
  validate(payload, second, false);
  require(same_outcome(first.outcome, second.outcome));
  require(first.sink_calls == second.sink_calls);
  require(first.loads == second.loads);
  require(first.last_type == second.last_type);

  const observation stopped = decode(payload, true);
  validate(payload, stopped, true);
  if (first.outcome.status == feedforge::decode_status::emitted) {
    require(stopped.outcome.status == feedforge::decode_status::stopped);
    require(stopped.loads == first.loads);
  } else {
    require(same_outcome(stopped.outcome, first.outcome));
    require(stopped.sink_calls == 0U);
  }
}

} // namespace

int feedforge_fuzz_decode_one_input(const std::uint8_t* const data,
                                    const std::size_t size) noexcept {
  const auto raw = std::span<const std::byte>{reinterpret_cast<const std::byte*>(data), size};
  exercise_decode(raw);

  std::array<std::byte, 4096U> decoded_storage{};
  std::span<const std::byte> decoded;
  if (feedforge::fuzz::decode_hex_seed(std::span<const std::uint8_t>{data, size}, decoded_storage,
                                       decoded)) {
    exercise_decode(decoded);
  }
  return 0;
}

#if defined(FEEDFORGE_FUZZ_STANDALONE)
int main() { return feedforge::fuzz::run_standalone_smoke(feedforge_fuzz_decode_one_input); }
#else
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size) noexcept {
  return feedforge_fuzz_decode_one_input(data, size);
}
#endif
