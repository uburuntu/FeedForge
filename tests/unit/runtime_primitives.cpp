#include "test_support.hpp"

#include <feedforge/config.hpp>
#include <feedforge/flow.hpp>
#include <feedforge/profile/concepts.hpp>
#include <feedforge/profile/portable_checked.hpp>
#include <feedforge/result.hpp>
#include <feedforge/types/ascii.hpp>
#include <feedforge/types/decimal.hpp>
#include <feedforge/types/identifiers.hpp>
#include <feedforge/types/timestamp.hpp>
#include <feedforge/version.hpp>
#include <feedforge/wire/load_big_endian.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace {

using feedforge::wire::load_unsigned;

constexpr std::array<std::byte, 8> sequence{
    std::byte{0x01}, std::byte{0x23}, std::byte{0x45}, std::byte{0x67},
    std::byte{0x89}, std::byte{0xAB}, std::byte{0xCD}, std::byte{0xEF}};

static_assert(load_unsigned<1>(sequence.data()) == UINT64_C(0x01));
static_assert(load_unsigned<2>(sequence.data()) == UINT64_C(0x0123));
static_assert(load_unsigned<4>(sequence.data()) == UINT64_C(0x01234567));
static_assert(load_unsigned<6>(sequence.data()) == UINT64_C(0x0123456789AB));
static_assert(load_unsigned<8>(sequence.data()) == UINT64_C(0x0123456789ABCDEF));

constexpr std::array<std::byte, 8> zero{};
constexpr std::array<std::byte, 8> ones{
    std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF},
    std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}};
constexpr std::array<std::byte, 8> alternating{
    std::byte{0xAA}, std::byte{0x55}, std::byte{0xAA}, std::byte{0x55},
    std::byte{0xAA}, std::byte{0x55}, std::byte{0xAA}, std::byte{0x55}};

static_assert(load_unsigned<8>(zero.data()) == UINT64_C(0));
static_assert(load_unsigned<1>(zero.data()) == UINT64_C(0));
static_assert(load_unsigned<2>(zero.data()) == UINT64_C(0));
static_assert(load_unsigned<4>(zero.data()) == UINT64_C(0));
static_assert(load_unsigned<6>(zero.data()) == UINT64_C(0));
static_assert(load_unsigned<1>(ones.data()) == UINT64_C(0xFF));
static_assert(load_unsigned<2>(ones.data()) == UINT64_C(0xFFFF));
static_assert(load_unsigned<4>(ones.data()) == UINT64_C(0xFFFFFFFF));
static_assert(load_unsigned<6>(ones.data()) == UINT64_C(0xFFFFFFFFFFFF));
static_assert(load_unsigned<8>(ones.data()) == UINT64_MAX);
static_assert(load_unsigned<1>(alternating.data()) == UINT64_C(0xAA));
static_assert(load_unsigned<2>(alternating.data()) == UINT64_C(0xAA55));
static_assert(load_unsigned<4>(alternating.data()) == UINT64_C(0xAA55AA55));
static_assert(load_unsigned<6>(alternating.data()) == UINT64_C(0xAA55AA55AA55));
static_assert(load_unsigned<8>(alternating.data()) == UINT64_C(0xAA55AA55AA55AA55));

static_assert(feedforge::wire::supported_unsigned_width<1>);
static_assert(feedforge::wire::supported_unsigned_width<2>);
static_assert(feedforge::wire::supported_unsigned_width<4>);
static_assert(feedforge::wire::supported_unsigned_width<6>);
static_assert(feedforge::wire::supported_unsigned_width<8>);
static_assert(!feedforge::wire::supported_unsigned_width<0>);
static_assert(!feedforge::wire::supported_unsigned_width<3>);
static_assert(!feedforge::wire::supported_unsigned_width<7>);

constexpr feedforge::timestamp_ns timestamp{load_unsigned<6>(ones.data())};
static_assert(timestamp.value == UINT64_C(0xFFFFFFFFFFFF));

constexpr feedforge::decimal<std::uint32_t, 4> price{UINT32_C(123456789)};
static_assert(price.raw == UINT32_C(123456789));
static_assert(decltype(price)::scale == 4);

constexpr feedforge::ascii<8> padded{{'F', 'O', 'O', ' ', ' ', ' ', ' ', ' '}};
constexpr feedforge::ascii<4> leading{{' ', 'A', ' ', ' '}};
constexpr feedforge::ascii<4> embedded_nul{{'A', '\0', ' ', ' '}};
constexpr feedforge::ascii<4> all_spaces{{' ', ' ', ' ', ' '}};
constexpr feedforge::ascii<4> unpadded{{'A', '\0', 'B', '\t'}};

static_assert(padded.trimmed() == "FOO");
static_assert(leading.trimmed() == " A");
static_assert(embedded_nul.trimmed().size() == 2);
static_assert(embedded_nul.trimmed()[1] == '\0');
static_assert(all_spaces.trimmed().empty());
static_assert(unpadded.trimmed().size() == 4);
static_assert(unpadded.trimmed()[3] == '\t');
static_assert(padded.trimmed().data() == padded.raw.data());

static_assert(feedforge::stock_locate{UINT16_C(7)}.value == UINT16_C(7));
static_assert(feedforge::tracking_number{UINT16_C(8)}.value == UINT16_C(8));
static_assert(feedforge::order_reference_number{UINT64_C(9)}.value == UINT64_C(9));
static_assert(feedforge::match_number{UINT64_C(10)}.value == UINT64_C(10));
static_assert(feedforge::share_count{UINT32_C(11)}.value == UINT32_C(11));
static_assert(!std::same_as<feedforge::stock_locate, feedforge::tracking_number>);
static_assert(!std::is_convertible_v<std::uint16_t, feedforge::stock_locate>);

static_assert(std::is_standard_layout_v<feedforge::timestamp_ns>);
static_assert(std::is_trivially_copyable_v<feedforge::timestamp_ns>);
static_assert(std::is_standard_layout_v<decltype(price)>);
static_assert(std::is_trivially_copyable_v<decltype(price)>);
static_assert(std::is_standard_layout_v<decltype(padded)>);
static_assert(std::is_trivially_copyable_v<decltype(padded)>);
static_assert(std::is_standard_layout_v<feedforge::stock_locate>);
static_assert(std::is_trivially_copyable_v<feedforge::stock_locate>);

constexpr feedforge::decode_outcome emitted{
    feedforge::decode_status::emitted, std::byte{'A'}, 36, 36};
constexpr feedforge::decode_outcome stopped{
    feedforge::decode_status::stopped, std::byte{'A'}, 36, 36};
constexpr feedforge::decode_outcome empty{
    feedforge::decode_status::empty_payload, std::byte{0}, 0, 0};
constexpr feedforge::decode_outcome unknown{
    feedforge::decode_status::unknown_message_type, std::byte{'?'}, 0, 1};
constexpr feedforge::decode_outcome wrong_size{
    feedforge::decode_status::invalid_message_size, std::byte{'A'}, 36, 35};
constexpr feedforge::decode_outcome known_skipped{
    feedforge::decode_status::known_unselected_skipped, std::byte{'S'}, 12, 12};
constexpr feedforge::decode_outcome unknown_skipped{
    feedforge::decode_status::unknown_skipped, std::byte{'?'}, 0, 1};

static_assert(!emitted.is_error() && !emitted.is_terminal());
static_assert(!known_skipped.is_error() && !known_skipped.is_terminal());
static_assert(!unknown_skipped.is_error() && !unknown_skipped.is_terminal());
static_assert(!stopped.is_error() && stopped.is_terminal());
static_assert(empty.is_error() && empty.is_terminal());
static_assert(unknown.is_error() && unknown.is_terminal());
static_assert(wrong_size.is_error() && wrong_size.is_terminal());
static_assert(std::is_trivially_copyable_v<feedforge::decode_outcome>);
static_assert(std::is_trivially_copyable_v<feedforge::replay_summary>);
static_assert(std::same_as<decltype(feedforge::replay_summary{}.bytes_consumed), std::uint64_t>);
static_assert(std::same_as<decltype(feedforge::replay_summary{}.error_offset), std::uint64_t>);

struct test_implementation {
  static constexpr std::string_view variant_id = "test.v1";

  template <std::size_t Width>
  [[nodiscard]] static constexpr std::uint64_t
  load_unsigned(std::byte const* first) noexcept {
    return feedforge::wire::load_unsigned<Width>(first);
  }
};

static_assert(feedforge::decoder_implementation<test_implementation>);
static_assert(feedforge::decoder_implementation<feedforge::profile::portable_checked>);
static_assert(feedforge::profile::portable_checked::variant_id ==
              "portable_checked.v1");

struct test_event {};

struct accepting_sink {
  feedforge::flow operator()(test_event const&) noexcept {
    return feedforge::flow::continue_;
  }
};

struct throwing_sink {
  feedforge::flow operator()(test_event const&) {
    return feedforge::flow::continue_;
  }
};

struct wrong_result_sink {
  bool operator()(test_event const&) noexcept { return true; }
};

static_assert(feedforge::sink_for<accepting_sink, test_event>);
static_assert(!feedforge::sink_for<throwing_sink, test_event>);
static_assert(!feedforge::sink_for<wrong_result_sink, test_event>);
static_assert(feedforge::runtime_api_epoch == 1);
static_assert(feedforge::runtime_api_revision == 0);
static_assert(feedforge::version_string == "0.5.0");

}  // namespace

int main() {
  std::array<std::byte, 8> input = sequence;
  FEEDFORGE_CHECK(feedforge::profile::portable_checked::load_unsigned<8>(input.data()) ==
         UINT64_C(0x0123456789ABCDEF));

  feedforge::replay_summary summary{};
  summary.status = feedforge::replay_status::stopped;
  summary.frames_seen = 1;
  FEEDFORGE_CHECK(summary.status == feedforge::replay_status::stopped);
  FEEDFORGE_CHECK(summary.frames_seen == 1);
}
