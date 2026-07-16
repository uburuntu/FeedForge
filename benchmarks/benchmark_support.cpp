#include "benchmark_support.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

#if defined(__APPLE__) || defined(__linux__)
#include <sys/utsname.h>
#include <unistd.h>
#endif

#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif

#if defined(__linux__)
#include <sched.h>
#endif

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

namespace feedforge::benchmark {
namespace {

constexpr std::array fixture_files{
    std::string_view{"01_system_event.toml"},
    std::string_view{"02_stock_directory.toml"},
    std::string_view{"03_stock_trading_action.toml"},
    std::string_view{"04_reg_sho_restriction.toml"},
    std::string_view{"05_market_participant_position.toml"},
    std::string_view{"06_mwcb_decline_level.toml"},
    std::string_view{"07_mwcb_status.toml"},
    std::string_view{"08_ipo_quoting_period_update.toml"},
    std::string_view{"09_luld_auction_collar.toml"},
    std::string_view{"10_operational_halt.toml"},
    std::string_view{"11_add_order.toml"},
    std::string_view{"12_add_order_mpid.toml"},
    std::string_view{"13_order_executed.toml"},
    std::string_view{"14_order_executed_with_price.toml"},
    std::string_view{"15_order_cancel.toml"},
    std::string_view{"16_order_delete.toml"},
    std::string_view{"17_order_replace.toml"},
    std::string_view{"18_trade.toml"},
    std::string_view{"19_cross_trade.toml"},
    std::string_view{"20_broken_trade.toml"},
    std::string_view{"21_net_order_imbalance_indicator.toml"},
    std::string_view{"22_retail_price_improvement_indicator.toml"},
    std::string_view{"23_direct_listing_with_capital_raise.toml"},
};

[[nodiscard]] constexpr std::string_view trim(std::string_view value) noexcept {
  while (!value.empty() &&
         (value.front() == ' ' || value.front() == '\t' || value.front() == '\r' ||
          value.front() == '\n')) {
    value.remove_prefix(1U);
  }
  while (!value.empty() &&
         (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' ||
          value.back() == '\n')) {
    value.remove_suffix(1U);
  }
  return value;
}

[[nodiscard]] std::string manifest_value(std::string_view line) {
  const std::size_t equals = line.find('=');
  if (equals == std::string_view::npos) {
    return {};
  }
  std::string_view value = trim(line.substr(equals + 1U));
  if (value.size() >= 2U && value.front() == '"' && value.back() == '"') {
    value.remove_prefix(1U);
    value.remove_suffix(1U);
  }
  return std::string{value};
}

[[nodiscard]] std::uint64_t parse_unsigned(std::string_view text,
                                           std::string_view context) {
  std::uint64_t value{};
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
    throw std::runtime_error("invalid unsigned value in " + std::string{context});
  }
  return value;
}

[[nodiscard]] std::vector<std::byte> parse_hex(std::string_view text,
                                                std::string_view context) {
  std::vector<std::byte> result;
  std::size_t position{};
  while (position < text.size()) {
    while (position < text.size() && (text[position] == ' ' || text[position] == '\t')) {
      ++position;
    }
    if (position == text.size()) {
      break;
    }
    std::size_t token_end = position;
    while (token_end < text.size() && text[token_end] != ' ' && text[token_end] != '\t') {
      ++token_end;
    }
    unsigned int value{};
    const auto parsed =
        std::from_chars(text.data() + position, text.data() + token_end, value, 16);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + token_end || value > 0xffU) {
      throw std::runtime_error("invalid raw_hex byte in " + std::string{context});
    }
    result.push_back(std::byte{static_cast<unsigned char>(value)});
    position = token_end;
  }
  return result;
}

[[nodiscard]] fixture load_fixture(const std::filesystem::path& directory,
                                   std::string_view file_name) {
  const std::filesystem::path path = directory / file_name;
  std::ifstream input{path};
  if (!input) {
    throw std::runtime_error("unable to read benchmark fixture: " + path.string());
  }

  enum class section { top, expected_order_events, other };
  section current = section::top;
  fixture loaded;
  loaded.file_name = std::string{file_name};
  std::string author;
  std::string order_result;
  std::uint64_t format_version{};

  std::string line;
  while (std::getline(input, line)) {
    const std::string_view current_line = trim(line);
    if (current_line.empty() || current_line.front() == '#') {
      continue;
    }
    if (current_line.front() == '[') {
      current = current_line == "[expected_order_events]" ? section::expected_order_events
                                                          : section::other;
      continue;
    }

    const std::size_t equals = current_line.find('=');
    if (equals == std::string_view::npos) {
      continue;
    }
    const std::string_view key = trim(current_line.substr(0U, equals));
    const std::string value = manifest_value(current_line);
    if (current == section::top) {
      if (key == "format_version") {
        format_version = parse_unsigned(value, file_name);
      } else if (key == "message_type") {
        if (value.size() != 1U) {
          throw std::runtime_error("message_type is not one byte in " +
                                   std::string{file_name});
        }
        loaded.message_type = value.front();
      } else if (key == "message_name") {
        loaded.message_name = value;
      } else if (key == "author") {
        author = value;
      } else if (key == "reviewer") {
        loaded.reviewer = value;
      } else if (key == "review_status") {
        loaded.review_status = value;
      } else if (key == "byte_source") {
        loaded.byte_source = value;
      } else if (key == "raw_hex") {
        loaded.payload = parse_hex(value, file_name);
      } else if (key == "raw_size") {
        loaded.raw_size = static_cast<std::size_t>(parse_unsigned(value, file_name));
      }
    } else if (current == section::expected_order_events && key == "result") {
      order_result = value;
    }
  }

  const auto invalid = [&file_name](std::string_view reason) {
    throw std::runtime_error(std::string{reason} + " in " + std::string{file_name});
  };
  if (format_version != 1U) {
    invalid("fixture format_version is not 1");
  }
  if (loaded.payload.size() != loaded.raw_size) {
    invalid("raw_size does not match raw_hex");
  }
  if (loaded.payload.empty() ||
      loaded.payload.front() != std::byte{static_cast<unsigned char>(loaded.message_type)}) {
    invalid("message_type does not match raw_hex");
  }
  if (loaded.message_name.empty()) {
    invalid("message_name is empty");
  }
  if (loaded.review_status != "approved" || loaded.reviewer.empty() ||
      loaded.reviewer == author) {
    invalid("fixture lacks an independent approved review");
  }
  if (loaded.byte_source.find("hand-authored") == std::string::npos ||
      loaded.byte_source.find("not schema-generated") == std::string::npos) {
    invalid("fixture byte_source is not independently hand-authored");
  }
  if (order_result != "emit" && order_result != "skip") {
    invalid("expected_order_events.result is not emit or skip");
  }
  loaded.order_events_selected = order_result == "emit";
  loaded.sha256 = sha256_hex(loaded.payload);
  return loaded;
}

void append_u32(std::vector<std::byte>& output, std::uint32_t value) {
  output.push_back(std::byte{static_cast<unsigned char>((value >> 24U) & 0xffU)});
  output.push_back(std::byte{static_cast<unsigned char>((value >> 16U) & 0xffU)});
  output.push_back(std::byte{static_cast<unsigned char>((value >> 8U) & 0xffU)});
  output.push_back(std::byte{static_cast<unsigned char>(value & 0xffU)});
}

void append_counted(std::vector<std::byte>& output, std::string_view value) {
  if (value.size() > std::numeric_limits<std::uint32_t>::max()) {
    throw std::runtime_error("value is too large for benchmark corpus hash");
  }
  append_u32(output, static_cast<std::uint32_t>(value.size()));
  for (const char character : value) {
    output.push_back(std::byte{static_cast<unsigned char>(character)});
  }
}

[[nodiscard]] workload make_workload(std::string name, std::vector<std::size_t> indices,
                                     const corpus& input) {
  workload result;
  result.name = std::move(name);
  result.fixture_indices = std::move(indices);

  std::vector<std::byte> decode_bytes;
  append_counted(decode_bytes, result.name);
  for (const std::size_t index : result.fixture_indices) {
    if (index >= input.fixtures.size()) {
      throw std::runtime_error("benchmark workload fixture index is out of range");
    }
    const fixture& current = input.fixtures[index];
    if (current.payload.size() > std::numeric_limits<std::uint16_t>::max()) {
      throw std::runtime_error("benchmark fixture exceeds BinaryFILE frame length");
    }
    const auto size = static_cast<std::uint16_t>(current.payload.size());
    const std::byte high{static_cast<unsigned char>((size >> 8U) & 0xffU)};
    const std::byte low{static_cast<unsigned char>(size & 0xffU)};

    decode_bytes.push_back(high);
    decode_bytes.push_back(low);
    decode_bytes.insert(decode_bytes.end(), current.payload.begin(), current.payload.end());
    result.binary_file.push_back(high);
    result.binary_file.push_back(low);
    result.binary_file.insert(result.binary_file.end(), current.payload.begin(),
                              current.payload.end());

    result.payload_bytes += static_cast<std::uint64_t>(current.payload.size());
    ++result.messages;
    if (current.order_events_selected) {
      ++result.selected_events;
    } else {
      ++result.unselected_messages;
    }
  }
  result.binary_file.push_back(std::byte{0x00});
  result.binary_file.push_back(std::byte{0x00});
  result.framed_bytes = static_cast<std::uint64_t>(result.binary_file.size());
  result.decode_sha256 = sha256_hex(decode_bytes);
  result.replay_sha256 = sha256_hex(result.binary_file);
  return result;
}

class sha256 {
 public:
  void update(std::span<const std::byte> input) {
    for (const std::byte item : input) {
      buffer_[buffer_size_] = std::to_integer<std::uint8_t>(item);
      ++buffer_size_;
      ++total_bytes_;
      if (buffer_size_ == buffer_.size()) {
        transform(buffer_);
        buffer_size_ = 0U;
      }
    }
  }

  [[nodiscard]] std::array<std::uint8_t, 32U> finish() {
    const std::uint64_t bit_count = total_bytes_ * 8U;
    buffer_[buffer_size_++] = 0x80U;
    if (buffer_size_ > 56U) {
      std::fill(buffer_.begin() + static_cast<std::ptrdiff_t>(buffer_size_), buffer_.end(), 0U);
      transform(buffer_);
      buffer_size_ = 0U;
    }
    std::fill(buffer_.begin() + static_cast<std::ptrdiff_t>(buffer_size_),
              buffer_.begin() + 56, 0U);
    for (std::size_t index = 0U; index < 8U; ++index) {
      const std::size_t shift = (7U - index) * 8U;
      buffer_[56U + index] =
          static_cast<std::uint8_t>((bit_count >> shift) & 0xffU);
    }
    transform(buffer_);

    std::array<std::uint8_t, 32U> digest{};
    for (std::size_t word = 0U; word < state_.size(); ++word) {
      for (std::size_t byte = 0U; byte < 4U; ++byte) {
        const std::size_t shift = (3U - byte) * 8U;
        digest[word * 4U + byte] =
            static_cast<std::uint8_t>((state_[word] >> shift) & 0xffU);
      }
    }
    return digest;
  }

 private:
  static constexpr std::array<std::uint32_t, 64U> round_constants{
      0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU,
      0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U,
      0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U,
      0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
      0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U,
      0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
      0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
      0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
      0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U,
      0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U, 0x1e376c08U,
      0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU,
      0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
      0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
  };

  void transform(const std::array<std::uint8_t, 64U>& block) {
    std::array<std::uint32_t, 64U> words{};
    for (std::size_t index = 0U; index < 16U; ++index) {
      const std::size_t offset = index * 4U;
      words[index] = (static_cast<std::uint32_t>(block[offset]) << 24U) |
                     (static_cast<std::uint32_t>(block[offset + 1U]) << 16U) |
                     (static_cast<std::uint32_t>(block[offset + 2U]) << 8U) |
                     static_cast<std::uint32_t>(block[offset + 3U]);
    }
    for (std::size_t index = 16U; index < words.size(); ++index) {
      const std::uint32_t s0 =
          std::rotr(words[index - 15U], 7) ^ std::rotr(words[index - 15U], 18) ^
          (words[index - 15U] >> 3U);
      const std::uint32_t s1 =
          std::rotr(words[index - 2U], 17) ^ std::rotr(words[index - 2U], 19) ^
          (words[index - 2U] >> 10U);
      words[index] = words[index - 16U] + s0 + words[index - 7U] + s1;
    }

    std::uint32_t a = state_[0U];
    std::uint32_t b = state_[1U];
    std::uint32_t c = state_[2U];
    std::uint32_t d = state_[3U];
    std::uint32_t e = state_[4U];
    std::uint32_t f = state_[5U];
    std::uint32_t g = state_[6U];
    std::uint32_t h = state_[7U];
    for (std::size_t index = 0U; index < words.size(); ++index) {
      const std::uint32_t sum1 =
          std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
      const std::uint32_t choice = (e & f) ^ ((~e) & g);
      const std::uint32_t temporary1 =
          h + sum1 + choice + round_constants[index] + words[index];
      const std::uint32_t sum0 =
          std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
      const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
      const std::uint32_t temporary2 = sum0 + majority;
      h = g;
      g = f;
      f = e;
      e = d + temporary1;
      d = c;
      c = b;
      b = a;
      a = temporary1 + temporary2;
    }
    state_[0U] += a;
    state_[1U] += b;
    state_[2U] += c;
    state_[3U] += d;
    state_[4U] += e;
    state_[5U] += f;
    state_[6U] += g;
    state_[7U] += h;
  }

  std::array<std::uint32_t, 8U> state_{
      0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
      0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
  };
  std::array<std::uint8_t, 64U> buffer_{};
  std::size_t buffer_size_{};
  std::uint64_t total_bytes_{};
};

[[nodiscard]] std::string digest_hex(const std::array<std::uint8_t, 32U>& digest) {
  constexpr std::string_view digits{"0123456789abcdef"};
  std::string result;
  result.reserve(digest.size() * 2U);
  for (const std::uint8_t value : digest) {
    result.push_back(digits[(value >> 4U) & 0x0fU]);
    result.push_back(digits[value & 0x0fU]);
  }
  return result;
}

[[nodiscard]] double percentile(const std::vector<double>& sorted, double quantile) {
  if (sorted.empty()) {
    throw std::runtime_error("cannot summarize an empty sample");
  }
  const double position = quantile * static_cast<double>(sorted.size() - 1U);
  const auto lower = static_cast<std::size_t>(std::floor(position));
  const auto upper = static_cast<std::size_t>(std::ceil(position));
  const double fraction = position - static_cast<double>(lower);
  return sorted[lower] + ((sorted[upper] - sorted[lower]) * fraction);
}

#if defined(__linux__)
[[nodiscard]] std::string read_first_line(const std::filesystem::path& path) {
  std::ifstream input{path};
  std::string line;
  if (input && std::getline(input, line)) {
    return std::string{trim(line)};
  }
  return {};
}
#endif

#if defined(__APPLE__)
[[nodiscard]] std::string sysctl_string(const char* name) {
  std::size_t size{};
  if (sysctlbyname(name, nullptr, &size, nullptr, 0U) != 0 || size == 0U) {
    return {};
  }
  std::string result(size, '\0');
  if (sysctlbyname(name, result.data(), &size, nullptr, 0U) != 0) {
    return {};
  }
  while (!result.empty() && result.back() == '\0') {
    result.pop_back();
  }
  return result;
}

template <class Value>
[[nodiscard]] std::uint64_t sysctl_unsigned(const char* name) {
  Value value{};
  std::size_t size = sizeof(value);
  if (sysctlbyname(name, &value, &size, nullptr, 0U) != 0) {
    return 0U;
  }
  return static_cast<std::uint64_t>(value);
}
#endif

#if defined(__linux__)
[[nodiscard]] std::string cpuinfo_value(std::string_view wanted) {
  std::ifstream input{"/proc/cpuinfo"};
  std::string line;
  while (std::getline(input, line)) {
    const std::string_view view = trim(line);
    const std::size_t colon = view.find(':');
    if (colon == std::string_view::npos) {
      continue;
    }
    if (trim(view.substr(0U, colon)) == wanted) {
      return std::string{trim(view.substr(colon + 1U))};
    }
  }
  return {};
}

[[nodiscard]] std::uint64_t linux_memory_bytes() {
  std::ifstream input{"/proc/meminfo"};
  std::string line;
  while (std::getline(input, line)) {
    if (line.rfind("MemTotal:", 0U) != 0U) {
      continue;
    }
    std::istringstream stream{line.substr(std::string{"MemTotal:"}.size())};
    std::uint64_t kibibytes{};
    stream >> kibibytes;
    return kibibytes * 1024U;
  }
  return 0U;
}

[[nodiscard]] std::uint64_t linux_physical_cpus() {
  std::ifstream input{"/proc/cpuinfo"};
  std::set<std::pair<std::string, std::string>> cores;
  std::string physical;
  std::string core;
  std::string line;
  const auto commit = [&]() {
    if (!core.empty()) {
      cores.emplace(physical, core);
    }
    physical.clear();
    core.clear();
  };
  while (std::getline(input, line)) {
    const std::string_view view = trim(line);
    if (view.empty()) {
      commit();
      continue;
    }
    const std::size_t colon = view.find(':');
    if (colon == std::string_view::npos) {
      continue;
    }
    const std::string_view key = trim(view.substr(0U, colon));
    const std::string value{trim(view.substr(colon + 1U))};
    if (key == "physical id") {
      physical = value;
    } else if (key == "core id") {
      core = value;
    }
  }
  commit();
  return static_cast<std::uint64_t>(cores.size());
}

[[nodiscard]] std::string linux_affinity() {
  cpu_set_t allowed;
  CPU_ZERO(&allowed);
  if (sched_getaffinity(0, sizeof(allowed), &allowed) != 0) {
    return "unavailable";
  }
  std::ostringstream output;
  bool first = true;
  for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
    if (CPU_ISSET(cpu, &allowed) == 0) {
      continue;
    }
    if (!first) {
      output << ',';
    }
    output << cpu;
    first = false;
  }
  return output.str();
}
#endif

}  // namespace

corpus load_corpus(const std::filesystem::path& fixture_directory) {
  corpus result;
  result.fixtures.reserve(fixture_files.size());
  for (const std::string_view file_name : fixture_files) {
    result.fixtures.push_back(load_fixture(fixture_directory, file_name));
  }

  const auto selected = std::count_if(
      result.fixtures.begin(), result.fixtures.end(),
      [](const fixture& current) { return current.order_events_selected; });
  if (selected != 8 || result.fixtures.size() - static_cast<std::size_t>(selected) != 15U) {
    throw std::runtime_error("reviewed fixture corpus is not the frozen 8 selected/15 skipped set");
  }

  std::vector<std::byte> normalized;
  append_counted(normalized, "feedforge-benchmark-corpus-v1");
  for (const fixture& current : result.fixtures) {
    append_counted(normalized, current.file_name);
    append_u32(normalized, static_cast<std::uint32_t>(current.payload.size()));
    normalized.insert(normalized.end(), current.payload.begin(), current.payload.end());
    normalized.push_back(current.order_events_selected ? std::byte{0x01}
                                                       : std::byte{0x00});
  }
  result.sha256 = sha256_hex(normalized);
  return result;
}

std::vector<workload> make_workloads(const corpus& input) {
  std::vector<std::size_t> all;
  std::vector<std::size_t> selected;
  std::vector<std::size_t> unselected;
  all.reserve(input.fixtures.size());
  for (std::size_t index = 0U; index < input.fixtures.size(); ++index) {
    all.push_back(index);
    if (input.fixtures[index].order_events_selected) {
      selected.push_back(index);
    } else {
      unselected.push_back(index);
    }
  }

  std::vector<workload> result;
  result.reserve(4U);
  result.push_back(make_workload("all_types", all, input));
  result.push_back(make_workload("selected", std::move(selected), input));
  result.push_back(make_workload("unselected", std::move(unselected), input));
  result.push_back(make_workload("mixed", std::move(all), input));
  return result;
}

distribution summarize(const std::span<const double> values) {
  if (values.empty()) {
    throw std::runtime_error("cannot summarize an empty sample");
  }
  std::vector<double> sorted(values.begin(), values.end());
  if (std::any_of(sorted.begin(), sorted.end(),
                  [](double value) { return !std::isfinite(value); })) {
    throw std::runtime_error("benchmark sample contains a non-finite value");
  }
  std::sort(sorted.begin(), sorted.end());
  const double median = percentile(sorted, 0.50);
  std::vector<double> deviations;
  deviations.reserve(sorted.size());
  for (const double value : sorted) {
    deviations.push_back(std::abs(value - median));
  }
  std::sort(deviations.begin(), deviations.end());
  return distribution{
      sorted.front(),
      percentile(sorted, 0.05),
      median,
      percentile(sorted, 0.95),
      sorted.back(),
      percentile(deviations, 0.50),
  };
}

std::string sha256_hex(const std::span<const std::byte> bytes) {
  sha256 hasher;
  hasher.update(bytes);
  return digest_hex(hasher.finish());
}

std::string sha256_hex(const std::string_view text) {
  const auto bytes = std::as_bytes(std::span{text.data(), text.size()});
  return sha256_hex(bytes);
}

host_manifest read_host_manifest() {
  host_manifest result;
#if defined(__APPLE__) || defined(__linux__)
  utsname information{};
  if (uname(&information) == 0) {
    result.os = information.sysname;
    result.kernel = std::string{information.release} + " " + information.version;
    result.architecture = information.machine;
  }
#elif defined(_WIN32)
  result.os = "Windows";
  result.kernel = "not_recorded";
  const char* architecture = std::getenv("PROCESSOR_ARCHITECTURE");
  result.architecture = architecture == nullptr ? "unknown" : architecture;
#else
  result.os = "unknown";
  result.kernel = "unknown";
  result.architecture = "unknown";
#endif

#if defined(__APPLE__)
  result.cpu_model = sysctl_string("machdep.cpu.brand_string");
  result.machine_model = sysctl_string("hw.model");
  if (result.cpu_model.empty()) {
    result.cpu_model = result.machine_model;
  }
  result.logical_cpus = sysctl_unsigned<std::uint32_t>("hw.logicalcpu");
  result.physical_cpus = sysctl_unsigned<std::uint32_t>("hw.physicalcpu");
  result.memory_bytes = sysctl_unsigned<std::uint64_t>("hw.memsize");
  result.cpu_affinity = "unsupported_macos";
  result.cpu_governor = "not_exposed_macos";
  result.turbo_state = "not_exposed_macos";
  result.limitations.emplace_back(
      "macOS exposes no supported process CPU-affinity API; core placement is "
      "scheduler-controlled");
  result.limitations.emplace_back(
      "power mode, frequency, turbo, and thermal pressure are not captured by this harness");
#elif defined(__linux__)
  result.cpu_model = cpuinfo_value("model name");
  if (result.cpu_model.empty()) {
    result.cpu_model = cpuinfo_value("Hardware");
  }
  result.machine_model = cpuinfo_value("Model");
  const long online = sysconf(_SC_NPROCESSORS_ONLN);
  result.logical_cpus = online > 0 ? static_cast<std::uint64_t>(online) : 0U;
  result.physical_cpus = linux_physical_cpus();
  if (result.physical_cpus == 0U) {
    result.physical_cpus = result.logical_cpus;
    result.limitations.emplace_back(
        "physical core count could not be distinguished from logical CPUs");
  }
  result.memory_bytes = linux_memory_bytes();
  result.cpu_affinity = linux_affinity();
  result.cpu_governor =
      read_first_line("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
  result.turbo_state =
      read_first_line("/sys/devices/system/cpu/intel_pstate/no_turbo");
  if (result.turbo_state.empty()) {
    result.turbo_state = read_first_line("/sys/devices/system/cpu/cpufreq/boost");
  }
  if (result.cpu_governor.empty()) {
    result.cpu_governor = "unavailable";
    result.limitations.emplace_back("CPU frequency governor was unavailable");
  }
  if (result.turbo_state.empty()) {
    result.turbo_state = "unavailable";
    result.limitations.emplace_back("CPU boost/turbo state was unavailable");
  }
#elif defined(_WIN32)
  SYSTEM_INFO system{};
  GetNativeSystemInfo(&system);
  result.logical_cpus = static_cast<std::uint64_t>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
  result.physical_cpus = 0U;
  MEMORYSTATUSEX memory{};
  memory.dwLength = sizeof(memory);
  if (GlobalMemoryStatusEx(&memory) != 0) {
    result.memory_bytes = memory.ullTotalPhys;
  }
  const char* identifier = std::getenv("PROCESSOR_IDENTIFIER");
  result.cpu_model = identifier == nullptr ? "unknown" : identifier;
  result.machine_model = "not_recorded";
  DWORD_PTR process_mask{};
  DWORD_PTR system_mask{};
  if (GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask) != 0) {
    std::ostringstream mask;
    mask << "0x" << std::hex << process_mask;
    result.cpu_affinity = mask.str();
  } else {
    result.cpu_affinity = "unavailable";
  }
  result.cpu_governor = "not_recorded";
  result.turbo_state = "not_recorded";
  result.limitations.emplace_back("physical core count and Windows power plan are not captured");
#endif

  if (result.os.empty()) {
    result.os = "unknown";
  }
  if (result.kernel.empty()) {
    result.kernel = "unknown";
  }
  if (result.architecture.empty()) {
    result.architecture = "unknown";
  }
  if (result.cpu_model.empty()) {
    result.cpu_model = "unknown";
    result.limitations.emplace_back("CPU model was unavailable");
  }
  if (result.machine_model.empty()) {
    result.machine_model = "unknown";
  }
  return result;
}

std::string utc_timestamp() {
  const std::time_t now = std::chrono::system_clock::to_time_t(
      std::chrono::system_clock::now());
  std::tm utc{};
#if defined(_WIN32)
  gmtime_s(&utc, &now);
#else
  gmtime_r(&now, &utc);
#endif
  std::ostringstream output;
  output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
  return output.str();
}

std::string json_escape(const std::string_view value) {
  constexpr std::string_view digits{"0123456789abcdef"};
  std::string result;
  result.reserve(value.size() + 2U);
  result.push_back('"');
  for (const char item : value) {
    const auto character = static_cast<unsigned char>(item);
    switch (character) {
      case '"':
        result.append("\\\"");
        break;
      case '\\':
        result.append("\\\\");
        break;
      case '\b':
        result.append("\\b");
        break;
      case '\f':
        result.append("\\f");
        break;
      case '\n':
        result.append("\\n");
        break;
      case '\r':
        result.append("\\r");
        break;
      case '\t':
        result.append("\\t");
        break;
      default:
        if (character < 0x20U) {
          result.append("\\u00");
          result.push_back(digits[(character >> 4U) & 0x0fU]);
          result.push_back(digits[character & 0x0fU]);
        } else {
          result.push_back(static_cast<char>(character));
        }
        break;
    }
  }
  result.push_back('"');
  return result;
}

std::string csv_escape(const std::string_view value) {
  std::string result;
  result.reserve(value.size() + 2U);
  result.push_back('"');
  for (const char character : value) {
    if (character == '"') {
      result.push_back('"');
    }
    result.push_back(character);
  }
  result.push_back('"');
  return result;
}

std::string join_command(const std::span<char* const> arguments) {
  std::ostringstream output;
  for (std::size_t index = 0U; index < arguments.size(); ++index) {
    if (index != 0U) {
      output << ' ';
    }
    const std::string_view argument{arguments[index]};
    const bool simple =
        !argument.empty() &&
        argument.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_./:-=") ==
            std::string_view::npos;
    if (simple) {
      output << argument;
      continue;
    }
    output << '\'';
    for (const char character : argument) {
      if (character == '\'') {
        output << "'\\''";
      } else {
        output << character;
      }
    }
    output << '\'';
  }
  return output.str();
}

void atomic_write(const std::filesystem::path& path, const std::string_view contents) {
  if (path.empty()) {
    return;
  }
  const std::filesystem::path parent = path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }
  const std::filesystem::path temporary = path.string() + ".tmp";
  {
    std::ofstream output{temporary, std::ios::binary | std::ios::trunc};
    if (!output) {
      throw std::runtime_error("unable to create result artifact: " + temporary.string());
    }
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.flush();
    if (!output) {
      throw std::runtime_error("unable to write result artifact: " + temporary.string());
    }
  }
  std::error_code error;
  std::filesystem::remove(path, error);
  error.clear();
  std::filesystem::rename(temporary, path, error);
  if (error) {
    std::filesystem::remove(temporary);
    throw std::runtime_error("unable to publish result artifact " + path.string() + ": " +
                             error.message());
  }
}

}  // namespace feedforge::benchmark
