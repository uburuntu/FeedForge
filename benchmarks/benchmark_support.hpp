#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace feedforge::benchmark {

inline constexpr std::string_view contract_version{"1.0.0"};
inline constexpr std::uint32_t result_schema_version{1U};

struct fixture {
  std::string file_name;
  std::string message_name;
  std::string reviewer;
  std::string review_status;
  std::string byte_source;
  char message_type{};
  std::vector<std::byte> payload;
  std::size_t raw_size{};
  bool order_events_selected{};
  std::string sha256;
};

struct corpus {
  std::vector<fixture> fixtures;
  std::string sha256;
};

struct workload {
  std::string name;
  std::vector<std::size_t> fixture_indices;
  std::vector<std::byte> binary_file;
  std::uint64_t payload_bytes{};
  std::uint64_t framed_bytes{};
  std::uint64_t messages{};
  std::uint64_t selected_events{};
  std::uint64_t unselected_messages{};
  std::string decode_sha256;
  std::string replay_sha256;
};

struct distribution {
  double minimum{};
  double p05{};
  double median{};
  double p95{};
  double maximum{};
  double mad{};
};

struct host_manifest {
  std::string os;
  std::string kernel;
  std::string architecture;
  std::string cpu_model;
  std::string machine_model;
  std::string cpu_affinity;
  std::string cpu_governor;
  std::string turbo_state;
  std::uint64_t logical_cpus{};
  std::uint64_t physical_cpus{};
  std::uint64_t memory_bytes{};
  std::vector<std::string> limitations;
};

[[nodiscard]] corpus load_corpus(const std::filesystem::path& fixture_directory);
[[nodiscard]] std::vector<workload> make_workloads(const corpus& input);
[[nodiscard]] distribution summarize(std::span<const double> values);
[[nodiscard]] std::string sha256_hex(std::span<const std::byte> bytes);
[[nodiscard]] std::string sha256_hex(std::string_view text);
[[nodiscard]] host_manifest read_host_manifest();
[[nodiscard]] std::string utc_timestamp();
[[nodiscard]] std::string json_escape(std::string_view value);
[[nodiscard]] std::string csv_escape(std::string_view value);
[[nodiscard]] std::string join_command(std::span<char* const> arguments);
void atomic_write(const std::filesystem::path& path, std::string_view contents);

}  // namespace feedforge::benchmark
