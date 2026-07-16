#include "benchmark_support.hpp"

#include <feedforge/generated/nasdaq/itch50_all.hpp>
#include <feedforge/generated/nasdaq/itch50_order_events.hpp>
#include <feedforge/version.hpp>

#include <feedforge_benchmark_build_config.hpp>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

namespace bench = feedforge::benchmark;
namespace all_messages = feedforge::generated::nasdaq::itch50_all;
namespace order_events = feedforge::generated::nasdaq::itch50_order_events;

using clock_type = std::chrono::steady_clock;

struct all_api {
  using decoder = all_messages::decoder;
  using metadata = all_messages::pipeline_metadata;

  template <class Sink>
  [[nodiscard]] static feedforge::replay_summary replay(
      const std::span<const std::byte> input, Sink& sink) noexcept {
    return all_messages::replay_binary_file(input, sink);
  }
};

struct order_api {
  using decoder = order_events::decoder;
  using metadata = order_events::pipeline_metadata;

  template <class Sink>
  [[nodiscard]] static feedforge::replay_summary replay(
      const std::span<const std::byte> input, Sink& sink) noexcept {
    return order_events::replay_binary_file(input, sink);
  }
};

enum class operation_kind : std::uint8_t { decode_one, replay_binary_file };
enum class pipeline_kind : std::uint8_t { all, order_events };

struct options {
  std::uint64_t batch{256U};
  std::size_t samples{15U};
  std::size_t warmup{5U};
  double minimum_time_ms{50.0};
  std::filesystem::path json_path;
  std::filesystem::path csv_path;
  bool smoke{};
};

struct case_definition {
  std::string id;
  operation_kind operation{};
  pipeline_kind pipeline{};
  const bench::workload* workload{};
  std::uint64_t bytes_per_round{};
  std::uint64_t messages_per_round{};
  std::uint64_t events_per_round{};
  std::string workload_sha256;
};

struct execution_result {
  std::uint64_t checksum{};
  std::uint64_t events{};
  std::uint64_t frames{};
};

struct timed_execution {
  std::uint64_t elapsed_ns{};
  execution_result execution;
};

struct sample {
  std::uint64_t elapsed_ns{};
  std::uint64_t rounds{};
  std::uint64_t bytes{};
  std::uint64_t messages{};
  std::uint64_t events{};
  std::uint64_t checksum{};
};

struct case_result {
  const case_definition* definition{};
  std::uint64_t rounds_per_sample{};
  std::vector<sample> samples;
  bench::distribution sample_time_ns;
  bench::distribution ns_per_message;
  std::optional<bench::distribution> ns_per_event;
  bench::distribution bytes_per_second;
  bench::distribution messages_per_second;
  std::optional<bench::distribution> events_per_second;
  double relative_mad{};
  double relative_p95_p05_spread{};
  bool implausible{};
  bool noisy{};
  std::vector<std::string> warnings;
};

struct report {
  options configuration;
  bench::corpus corpus;
  std::vector<bench::workload> workloads;
  std::vector<case_definition> cases;
  std::vector<case_result> results;
  bench::host_manifest host;
  std::string correctness_checksum;
  std::string timestamp;
  std::string command;
  std::vector<std::string> arguments;
  std::string working_directory;
  double timer_resolution_ns{};
  std::vector<std::string> warnings;
};

[[nodiscard]] constexpr std::uint64_t mix_checksum(std::uint64_t seed,
                                                   std::uint64_t value) noexcept {
  return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U));
}

template <class Value>
inline void do_not_optimize(const Value& value) noexcept {
#if defined(__GNUC__) || defined(__clang__)
  asm volatile("" : : "m"(value) : "memory");
#else
  std::atomic_signal_fence(std::memory_order_seq_cst);
  (void)value;
#endif
}

struct checksum_sink {
  std::uint64_t events{};
  std::uint64_t checksum{0xcbf29ce484222325ULL};

  template <class Event>
  feedforge::flow operator()(const Event& event) noexcept {
    do_not_optimize(event);
    ++events;
    checksum =
        mix_checksum(checksum, std::to_integer<std::uint64_t>(Event::source_discriminator));
    checksum = mix_checksum(checksum, events);
    return feedforge::flow::continue_;
  }
};

static_assert(all_messages::sink_for_all_selected_events<checksum_sink>);
static_assert(order_events::sink_for_all_selected_events<checksum_sink>);

volatile std::uint64_t escaped_checksum{};

[[nodiscard]] constexpr std::string_view operation_name(
    const operation_kind operation) noexcept {
  return operation == operation_kind::decode_one ? "decode_one" : "replay_binary_file";
}

[[nodiscard]] constexpr std::string_view pipeline_name(
    const pipeline_kind pipeline) noexcept {
  return pipeline == pipeline_kind::all ? "itch50_all" : "itch50_order_events";
}

[[nodiscard]] std::uint64_t checked_multiply(const std::uint64_t left,
                                             const std::uint64_t right,
                                             const std::string_view context) {
  if (right != 0U && left > std::numeric_limits<std::uint64_t>::max() / right) {
    throw std::runtime_error("counter overflow while measuring " + std::string{context});
  }
  return left * right;
}

[[nodiscard]] std::uint64_t parse_u64(const std::string_view value,
                                      const std::string_view option) {
  std::uint64_t result{};
  const auto parsed =
      std::from_chars(value.data(), value.data() + value.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
      result == 0U) {
    throw std::runtime_error(std::string{option} + " requires a positive integer");
  }
  return result;
}

[[nodiscard]] double parse_positive_double(const std::string_view value,
                                           const std::string_view option) {
  std::string storage{value};
  std::size_t consumed{};
  const double result = std::stod(storage, &consumed);
  if (consumed != storage.size() || !std::isfinite(result) || result <= 0.0) {
    throw std::runtime_error(std::string{option} + " requires a positive number");
  }
  return result;
}

void print_usage(const std::string_view executable) {
  std::cout
      << "Usage: " << executable << " [options]\n"
      << "  --samples N       recorded samples per case (default: 15)\n"
      << "  --warmup N        unrecorded warm-up samples per case (default: 5)\n"
      << "  --batch N         minimum workload rounds per sample (default: 256)\n"
      << "  --min-time-ms N   minimum calibrated sample duration (default: 50)\n"
      << "  --json PATH       write canonical JSON result artifact\n"
      << "  --csv PATH        write canonical CSV result artifact\n"
      << "  --smoke           fast correctness and measurement smoke mode\n"
      << "  --help            show this help\n";
}

[[nodiscard]] options parse_options(const int argc, char** argv) {
  options result;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    if (argument == "--help") {
      print_usage(argv[0]);
      std::exit(0);
    }
    if (argument == "--smoke") {
      result.smoke = true;
      continue;
    }
    const auto require_value = [&](const std::string_view name) -> std::string_view {
      if (index + 1 >= argc) {
        throw std::runtime_error(std::string{name} + " requires a value");
      }
      ++index;
      return argv[index];
    };
    if (argument == "--samples") {
      result.samples =
          static_cast<std::size_t>(parse_u64(require_value(argument), argument));
    } else if (argument == "--warmup") {
      result.warmup =
          static_cast<std::size_t>(parse_u64(require_value(argument), argument));
    } else if (argument == "--batch") {
      result.batch = parse_u64(require_value(argument), argument);
    } else if (argument == "--min-time-ms") {
      result.minimum_time_ms =
          parse_positive_double(require_value(argument), argument);
    } else if (argument == "--json") {
      result.json_path = require_value(argument);
    } else if (argument == "--csv") {
      result.csv_path = require_value(argument);
    } else {
      throw std::runtime_error("unknown benchmark option: " + std::string{argument});
    }
  }
  if (result.smoke) {
    result.samples = 3U;
    result.warmup = 1U;
    result.batch = 1U;
    result.minimum_time_ms = 2.0;
  }
  if (result.samples < 3U) {
    throw std::runtime_error("--samples must be at least 3");
  }
  return result;
}

[[nodiscard]] const bench::workload& find_workload(
    const std::vector<bench::workload>& workloads, const std::string_view name) {
  const auto found =
      std::find_if(workloads.begin(), workloads.end(), [name](const auto& workload) {
        return workload.name == name;
      });
  if (found == workloads.end()) {
    throw std::runtime_error("internal benchmark workload is missing: " +
                             std::string{name});
  }
  return *found;
}

[[nodiscard]] std::vector<case_definition> make_cases(
    const std::vector<bench::workload>& workloads) {
  const bench::workload& all_types = find_workload(workloads, "all_types");
  const bench::workload& selected = find_workload(workloads, "selected");
  const bench::workload& unselected = find_workload(workloads, "unselected");
  const bench::workload& mixed = find_workload(workloads, "mixed");

  std::vector<case_definition> result;
  const auto add = [&result](const operation_kind operation,
                             const pipeline_kind pipeline,
                             const bench::workload& workload) {
    const bool decode = operation == operation_kind::decode_one;
    const std::uint64_t events =
        pipeline == pipeline_kind::all ? workload.messages : workload.selected_events;
    result.push_back(case_definition{
        std::string{operation_name(operation)} + "/" +
            std::string{pipeline_name(pipeline)} + "/" + workload.name,
        operation,
        pipeline,
        &workload,
        decode ? workload.payload_bytes : workload.framed_bytes,
        workload.messages,
        events,
        decode ? workload.decode_sha256 : workload.replay_sha256,
    });
  };

  add(operation_kind::decode_one, pipeline_kind::all, all_types);
  add(operation_kind::replay_binary_file, pipeline_kind::all, all_types);
  for (const bench::workload* workload : {&selected, &unselected, &mixed}) {
    add(operation_kind::decode_one, pipeline_kind::order_events, *workload);
    add(operation_kind::replay_binary_file, pipeline_kind::order_events, *workload);
  }
  return result;
}

template <class Metadata>
[[nodiscard]] std::uint16_t expected_size(const std::byte message_type) {
  for (const auto& known : Metadata::known_messages) {
    if (known.discriminator == message_type) {
      return known.size;
    }
  }
  throw std::runtime_error("fixture message type is absent from generated metadata");
}

template <class Api>
void verify_decode_fixture(const bench::fixture& fixture, const bool selected,
                           std::ostringstream& normalized) {
  typename Api::decoder decoder;
  checksum_sink sink;
  const auto payload =
      std::span<const std::byte>{fixture.payload.data(), fixture.payload.size()};
  const feedforge::decode_outcome outcome = decoder.decode_one(payload, sink);
  const feedforge::decode_status wanted =
      selected ? feedforge::decode_status::emitted
               : feedforge::decode_status::known_unselected_skipped;
  const std::byte type{static_cast<unsigned char>(fixture.message_type)};
  const std::uint16_t size = expected_size<typename Api::metadata>(type);
  if (outcome.status != wanted || outcome.message_type != type ||
      outcome.expected_size != size || outcome.actual_size != fixture.payload.size() ||
      sink.events != (selected ? 1U : 0U)) {
    throw std::runtime_error("pre-timing decode correctness failed for " +
                             fixture.file_name);
  }
  normalized << fixture.file_name << ':' << static_cast<unsigned int>(outcome.status)
             << ':' << outcome.expected_size << ':' << outcome.actual_size << ':'
             << sink.events << ';';
}

template <class Api>
void verify_replay_workload(const bench::workload& workload,
                            const std::uint64_t expected_events,
                            std::ostringstream& normalized) {
  checksum_sink sink;
  const auto bytes = std::span<const std::byte>{workload.binary_file.data(),
                                                workload.binary_file.size()};
  const feedforge::replay_summary summary = Api::replay(bytes, sink);
  const std::uint64_t expected_skipped = workload.messages - expected_events;
  if (summary.status != feedforge::replay_status::complete ||
      summary.frames_seen != workload.messages ||
      summary.events_emitted != expected_events ||
      summary.known_messages_skipped != expected_skipped ||
      summary.unknown_messages_skipped != 0U ||
      summary.bytes_consumed != workload.binary_file.size() ||
      sink.events != expected_events) {
    throw std::runtime_error("pre-timing strict replay correctness failed for " +
                             workload.name);
  }
  normalized << workload.name << ':' << summary.frames_seen << ':'
             << summary.events_emitted << ':' << summary.known_messages_skipped << ':'
             << summary.bytes_consumed << ':' << sink.checksum << ';';
}

[[nodiscard]] std::string verify_correctness(
    const bench::corpus& corpus, const std::vector<bench::workload>& workloads) {
  if (corpus.fixtures.size() != all_api::metadata::known_messages.size() ||
      corpus.fixtures.size() != order_api::metadata::known_messages.size()) {
    throw std::runtime_error("fixture count does not match generated known-message tables");
  }

  std::ostringstream normalized;
  normalized << "feedforge-benchmark-correctness-v1;";
  for (const bench::fixture& fixture : corpus.fixtures) {
    verify_decode_fixture<all_api>(fixture, true, normalized);
    verify_decode_fixture<order_api>(fixture, fixture.order_events_selected, normalized);
  }
  for (const bench::workload& workload : workloads) {
    verify_replay_workload<all_api>(workload, workload.messages, normalized);
    verify_replay_workload<order_api>(workload, workload.selected_events, normalized);
  }

  const bench::workload& mixed = find_workload(workloads, "mixed");
  std::vector<std::byte> trailing = mixed.binary_file;
  trailing.push_back(std::byte{0xff});
  checksum_sink sink;
  const feedforge::replay_summary strict = order_api::replay(
      std::span<const std::byte>{trailing.data(), trailing.size()}, sink);
  if (strict.status != feedforge::replay_status::framing_error ||
      strict.framing_error != feedforge::framing_errc::trailing_data_after_end_marker ||
      strict.error_offset + 1U != trailing.size()) {
    throw std::runtime_error("pre-timing strict BinaryFILE trailing-data check failed");
  }
  normalized << "strict-trailing:" << strict.error_offset << ';';
  return bench::sha256_hex(normalized.str());
}

template <class Api>
[[nodiscard]] execution_result execute_decode(const case_definition& definition,
                                              const bench::corpus& corpus,
                                              const std::uint64_t rounds,
                                              typename Api::decoder& decoder,
                                              checksum_sink& sink) {
  std::uint64_t checksum{0x6a09e667f3bcc909ULL};
  for (std::uint64_t round = 0U; round < rounds; ++round) {
    for (const std::size_t index : definition.workload->fixture_indices) {
      const bench::fixture& fixture = corpus.fixtures[index];
      const auto payload =
          std::span<const std::byte>{fixture.payload.data(), fixture.payload.size()};
      do_not_optimize(payload);
      const feedforge::decode_outcome outcome = decoder.decode_one(payload, sink);
      checksum = mix_checksum(
          checksum, static_cast<std::uint64_t>(outcome.status));
      checksum =
          mix_checksum(checksum, std::to_integer<std::uint64_t>(outcome.message_type));
      checksum = mix_checksum(checksum, outcome.expected_size);
    }
    checksum = mix_checksum(checksum, round);
  }
  checksum = mix_checksum(checksum, sink.checksum);
  checksum = mix_checksum(checksum, sink.events);
  return execution_result{checksum, sink.events, 0U};
}

template <class Api>
[[nodiscard]] execution_result execute_replay(const case_definition& definition,
                                              const std::uint64_t rounds,
                                              checksum_sink& sink) {
  std::uint64_t checksum{0xbb67ae8584caa73bULL};
  std::uint64_t frames{};
  const auto input = std::span<const std::byte>{definition.workload->binary_file.data(),
                                                definition.workload->binary_file.size()};
  for (std::uint64_t round = 0U; round < rounds; ++round) {
    do_not_optimize(input);
    const feedforge::replay_summary summary = Api::replay(input, sink);
    frames += summary.frames_seen;
    checksum = mix_checksum(checksum, static_cast<std::uint64_t>(summary.status));
    checksum = mix_checksum(checksum, summary.frames_seen);
    checksum = mix_checksum(checksum, summary.events_emitted);
    checksum = mix_checksum(checksum, summary.known_messages_skipped);
    checksum = mix_checksum(checksum, summary.bytes_consumed);
    checksum = mix_checksum(checksum, round);
  }
  checksum = mix_checksum(checksum, sink.checksum);
  checksum = mix_checksum(checksum, sink.events);
  return execution_result{checksum, sink.events, frames};
}

template <class Api>
[[nodiscard]] timed_execution time_decode(const case_definition& definition,
                                          const bench::corpus& corpus,
                                          const std::uint64_t rounds) {
  typename Api::decoder decoder;
  checksum_sink sink;
  const clock_type::time_point start = clock_type::now();
  const execution_result execution =
      execute_decode<Api>(definition, corpus, rounds, decoder, sink);
  const clock_type::time_point finish = clock_type::now();
  const auto elapsed =
      std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
  if (elapsed < 0) {
    throw std::runtime_error("steady clock moved backwards");
  }
  escaped_checksum = execution.checksum;
  return timed_execution{static_cast<std::uint64_t>(elapsed), execution};
}

template <class Api>
[[nodiscard]] timed_execution time_replay(const case_definition& definition,
                                          const std::uint64_t rounds) {
  checksum_sink sink;
  const clock_type::time_point start = clock_type::now();
  const execution_result execution = execute_replay<Api>(definition, rounds, sink);
  const clock_type::time_point finish = clock_type::now();
  const auto elapsed =
      std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
  if (elapsed < 0) {
    throw std::runtime_error("steady clock moved backwards");
  }
  escaped_checksum = execution.checksum;
  return timed_execution{static_cast<std::uint64_t>(elapsed), execution};
}

[[nodiscard]] timed_execution time_case(const case_definition& definition,
                                        const bench::corpus& corpus,
                                        const std::uint64_t rounds) {
  if (definition.operation == operation_kind::decode_one) {
    return definition.pipeline == pipeline_kind::all
               ? time_decode<all_api>(definition, corpus, rounds)
               : time_decode<order_api>(definition, corpus, rounds);
  }
  return definition.pipeline == pipeline_kind::all
             ? time_replay<all_api>(definition, rounds)
             : time_replay<order_api>(definition, rounds);
}

void verify_timed_execution(const case_definition& definition,
                            const std::uint64_t rounds,
                            const timed_execution& measured) {
  const std::uint64_t expected_events =
      checked_multiply(definition.events_per_round, rounds, definition.id);
  if (measured.execution.events != expected_events) {
    throw std::runtime_error("timed event count changed for " + definition.id);
  }
  if (definition.operation == operation_kind::replay_binary_file) {
    const std::uint64_t expected_frames =
        checked_multiply(definition.messages_per_round, rounds, definition.id);
    if (measured.execution.frames != expected_frames) {
      throw std::runtime_error("timed replay frame count changed for " + definition.id);
    }
  }
  if (measured.execution.checksum == 0U) {
    throw std::runtime_error("anti-elision checksum is zero for " + definition.id);
  }
}

[[nodiscard]] std::uint64_t calibrate_rounds(const case_definition& definition,
                                             const bench::corpus& corpus,
                                             const options& configuration) {
  const double target =
      configuration.minimum_time_ms * 1'000'000.0;
  std::uint64_t rounds = configuration.batch;
  for (std::size_t attempt = 0U; attempt < 12U; ++attempt) {
    const timed_execution measured = time_case(definition, corpus, rounds);
    verify_timed_execution(definition, rounds, measured);
    if (static_cast<double>(measured.elapsed_ns) >= target) {
      return rounds;
    }
    const double observed =
        std::max(1.0, static_cast<double>(measured.elapsed_ns));
    const double requested_scale = std::ceil((target / observed) * 1.10);
    const auto scale = static_cast<std::uint64_t>(
        std::clamp(requested_scale, 2.0, 16.0));
    if (rounds > std::numeric_limits<std::uint64_t>::max() / scale) {
      throw std::runtime_error("calibrated batch overflow for " + definition.id);
    }
    rounds *= scale;
  }
  throw std::runtime_error("unable to calibrate sample duration for " + definition.id);
}

[[nodiscard]] case_result measure_case(const case_definition& definition,
                                       const bench::corpus& corpus,
                                       const options& configuration) {
  case_result result;
  result.definition = &definition;
  result.rounds_per_sample =
      calibrate_rounds(definition, corpus, configuration);

  for (std::size_t index = 0U; index < configuration.warmup; ++index) {
    const timed_execution measured =
        time_case(definition, corpus, result.rounds_per_sample);
    verify_timed_execution(definition, result.rounds_per_sample, measured);
  }

  result.samples.reserve(configuration.samples);
  std::optional<std::uint64_t> expected_checksum;
  for (std::size_t index = 0U; index < configuration.samples; ++index) {
    const timed_execution measured =
        time_case(definition, corpus, result.rounds_per_sample);
    verify_timed_execution(definition, result.rounds_per_sample, measured);
    if (!expected_checksum.has_value()) {
      expected_checksum = measured.execution.checksum;
    } else if (*expected_checksum != measured.execution.checksum) {
      throw std::runtime_error("anti-elision checksum changed between samples for " +
                               definition.id);
    }

    result.samples.push_back(sample{
        measured.elapsed_ns,
        result.rounds_per_sample,
        checked_multiply(definition.bytes_per_round, result.rounds_per_sample,
                         definition.id),
        checked_multiply(definition.messages_per_round, result.rounds_per_sample,
                         definition.id),
        checked_multiply(definition.events_per_round, result.rounds_per_sample,
                         definition.id),
        measured.execution.checksum,
    });
  }

  std::vector<double> sample_times;
  std::vector<double> ns_per_message;
  std::vector<double> ns_per_event;
  std::vector<double> bytes_per_second;
  std::vector<double> messages_per_second;
  std::vector<double> events_per_second;
  sample_times.reserve(result.samples.size());
  ns_per_message.reserve(result.samples.size());
  ns_per_event.reserve(result.samples.size());
  bytes_per_second.reserve(result.samples.size());
  messages_per_second.reserve(result.samples.size());
  events_per_second.reserve(result.samples.size());
  for (const sample& current : result.samples) {
    const double elapsed = static_cast<double>(current.elapsed_ns);
    const double seconds = elapsed / 1'000'000'000.0;
    sample_times.push_back(elapsed);
    ns_per_message.push_back(elapsed / static_cast<double>(current.messages));
    bytes_per_second.push_back(static_cast<double>(current.bytes) / seconds);
    messages_per_second.push_back(static_cast<double>(current.messages) / seconds);
    if (current.events != 0U) {
      ns_per_event.push_back(elapsed / static_cast<double>(current.events));
      events_per_second.push_back(static_cast<double>(current.events) / seconds);
    }
  }

  result.sample_time_ns = bench::summarize(sample_times);
  result.ns_per_message = bench::summarize(ns_per_message);
  result.bytes_per_second = bench::summarize(bytes_per_second);
  result.messages_per_second = bench::summarize(messages_per_second);
  if (!ns_per_event.empty()) {
    result.ns_per_event = bench::summarize(ns_per_event);
    result.events_per_second = bench::summarize(events_per_second);
  }
  result.relative_mad = result.ns_per_message.mad / result.ns_per_message.median;
  result.relative_p95_p05_spread =
      (result.ns_per_message.p95 - result.ns_per_message.p05) /
      result.ns_per_message.median;
  result.noisy =
      result.relative_mad > 0.05 || result.relative_p95_p05_spread > 0.20;
  const double target_ns = configuration.minimum_time_ms * 1'000'000.0;
  result.implausible =
      result.sample_time_ns.minimum < target_ns * 0.75 ||
      result.ns_per_message.median < 0.01 ||
      result.sample_time_ns.minimum <= 0.0;
  if (result.noisy) {
    result.warnings.emplace_back(
        "sample dispersion exceeds the 5% MAD or 20% p95-p05 diagnostic bound");
  }
  if (result.implausible) {
    result.warnings.emplace_back(
        "sample duration or per-message timing is implausible; discard this run");
  }
  return result;
}

[[nodiscard]] double timer_resolution_ns() {
  double minimum = std::numeric_limits<double>::infinity();
  clock_type::time_point previous = clock_type::now();
  for (std::size_t index = 0U; index < 10'000U; ++index) {
    const clock_type::time_point current = clock_type::now();
    const auto delta =
        std::chrono::duration<double, std::nano>(current - previous).count();
    if (delta > 0.0) {
      minimum = std::min(minimum, delta);
    }
    previous = current;
  }
  return std::isfinite(minimum) ? minimum : 0.0;
}

void append_distribution(std::ostringstream& output,
                         const bench::distribution& values) {
  output << "{\"mad\":" << values.mad << ",\"maximum\":" << values.maximum
         << ",\"median\":" << values.median << ",\"minimum\":" << values.minimum
         << ",\"p05\":" << values.p05 << ",\"p95\":" << values.p95 << '}';
}

[[nodiscard]] std::string make_json(const report& data) {
  std::ostringstream output;
  output.imbue(std::locale::classic());
  output << std::setprecision(17);
  output << "{\"benchmarks\":[";
  for (std::size_t index = 0U; index < data.results.size(); ++index) {
    if (index != 0U) {
      output << ',';
    }
    const case_result& result = data.results[index];
    const case_definition& definition = *result.definition;
    output << "{\"anti_elision_checksum\":"
           << bench::json_escape([&]() {
                std::ostringstream checksum;
                checksum << "0x" << std::hex << result.samples.front().checksum;
                return checksum.str();
              }())
           << ",\"bytes_per_round\":" << definition.bytes_per_round
           << ",\"events_per_round\":" << definition.events_per_round
           << ",\"id\":" << bench::json_escape(definition.id)
           << ",\"messages_per_round\":" << definition.messages_per_round
           << ",\"operation\":"
           << bench::json_escape(operation_name(definition.operation))
           << ",\"pipeline\":"
           << bench::json_escape(pipeline_name(definition.pipeline))
           << ",\"quality\":{\"implausible\":"
           << (result.implausible ? "true" : "false")
           << ",\"noisy\":" << (result.noisy ? "true" : "false")
           << ",\"relative_mad\":" << result.relative_mad
           << ",\"relative_p95_p05_spread\":"
           << result.relative_p95_p05_spread << ",\"warnings\":[";
    for (std::size_t warning = 0U; warning < result.warnings.size(); ++warning) {
      if (warning != 0U) {
        output << ',';
      }
      output << bench::json_escape(result.warnings[warning]);
    }
    output << "]},\"rounds_per_sample\":" << result.rounds_per_sample
           << ",\"samples\":[";
    for (std::size_t sample_index = 0U; sample_index < result.samples.size();
         ++sample_index) {
      if (sample_index != 0U) {
        output << ',';
      }
      const sample& current = result.samples[sample_index];
      std::ostringstream checksum;
      checksum << "0x" << std::hex << current.checksum;
      output << "{\"bytes\":" << current.bytes
             << ",\"checksum\":" << bench::json_escape(checksum.str())
             << ",\"elapsed_ns\":" << current.elapsed_ns
             << ",\"events\":" << current.events
             << ",\"messages\":" << current.messages
             << ",\"rounds\":" << current.rounds << '}';
    }
    output << "],\"statistics\":{\"bytes_per_second\":";
    append_distribution(output, result.bytes_per_second);
    output << ",\"events_per_second\":";
    if (result.events_per_second.has_value()) {
      append_distribution(output, *result.events_per_second);
    } else {
      output << "null";
    }
    output << ",\"messages_per_second\":";
    append_distribution(output, result.messages_per_second);
    output << ",\"ns_per_event\":";
    if (result.ns_per_event.has_value()) {
      append_distribution(output, *result.ns_per_event);
    } else {
      output << "null";
    }
    output << ",\"ns_per_message\":";
    append_distribution(output, result.ns_per_message);
    output << ",\"sample_time_ns\":";
    append_distribution(output, result.sample_time_ns);
    output << "},\"workload\":"
           << bench::json_escape(definition.workload->name)
           << ",\"workload_sha256\":"
           << bench::json_escape(definition.workload_sha256) << '}';
  }
  output << "],\"build\":{\"build_type\":"
         << bench::json_escape(bench::build_config::build_type)
         << ",\"compiler_builtin\":"
#if defined(__VERSION__)
         << bench::json_escape(__VERSION__)
#else
         << bench::json_escape("unavailable")
#endif
         << ",\"compiler_id\":"
         << bench::json_escape(bench::build_config::compiler_id)
         << ",\"compiler_path\":"
         << bench::json_escape(bench::build_config::compiler_path)
         << ",\"compiler_version\":"
         << bench::json_escape(bench::build_config::compiler_version)
         << ",\"config_flags\":"
         << bench::json_escape(bench::build_config::config_flags)
         << ",\"cxx_standard\":" << __cplusplus
         << ",\"feedforge_version\":" << bench::json_escape(feedforge::version_string)
         << ",\"generator\":" << bench::json_escape(bench::build_config::generator)
         << ",\"interprocedural_optimization\":"
         << bench::json_escape(bench::build_config::interprocedural_optimization)
         << ",\"pipeline_fingerprints\":{\"itch50_all\":"
         << bench::json_escape(all_messages::pipeline_metadata::pipeline_fingerprint)
         << ",\"itch50_order_events\":"
         << bench::json_escape(order_events::pipeline_metadata::pipeline_fingerprint)
         << "},\"schema_fingerprint\":"
         << bench::json_escape(all_messages::pipeline_metadata::schema_fingerprint)
         << ",\"target_flags\":"
         << bench::json_escape(bench::build_config::target_flags) << "},\"command\":{"
         << "\"argv\":[";
  for (std::size_t index = 0U; index < data.arguments.size(); ++index) {
    if (index != 0U) {
      output << ',';
    }
    output << bench::json_escape(data.arguments[index]);
  }
  output << "],\"joined\":" << bench::json_escape(data.command)
         << ",\"working_directory\":"
         << bench::json_escape(data.working_directory)
         << "},\"config\":{\"batch\":" << data.configuration.batch
         << ",\"clock\":\"std::chrono::steady_clock\",\"clock_is_steady\":"
         << (clock_type::is_steady ? "true" : "false")
         << ",\"minimum_time_ms\":" << data.configuration.minimum_time_ms
         << ",\"samples\":" << data.configuration.samples
         << ",\"smoke\":" << (data.configuration.smoke ? "true" : "false")
         << ",\"timer_resolution_ns\":" << data.timer_resolution_ns
         << ",\"warmup\":" << data.configuration.warmup
         << "},\"contract_version\":"
         << bench::json_escape(bench::contract_version)
         << ",\"corpus\":{\"fixture_count\":" << data.corpus.fixtures.size()
         << ",\"fixtures\":[";
  for (std::size_t index = 0U; index < data.corpus.fixtures.size(); ++index) {
    if (index != 0U) {
      output << ',';
    }
    const bench::fixture& fixture = data.corpus.fixtures[index];
    output << "{\"byte_source\":" << bench::json_escape(fixture.byte_source)
           << ",\"file\":" << bench::json_escape(fixture.file_name)
           << ",\"message_name\":" << bench::json_escape(fixture.message_name)
           << ",\"message_type\":"
           << bench::json_escape(std::string{fixture.message_type})
           << ",\"order_events_selected\":"
           << (fixture.order_events_selected ? "true" : "false")
           << ",\"review_status\":" << bench::json_escape(fixture.review_status)
           << ",\"reviewer\":" << bench::json_escape(fixture.reviewer)
           << ",\"sha256\":" << bench::json_escape(fixture.sha256)
           << ",\"size\":" << fixture.raw_size << '}';
  }
  output << "],\"sha256\":" << bench::json_escape(data.corpus.sha256)
         << ",\"source\":\"independently reviewed tests/fixtures/itch50 raw_hex\"}"
         << ",\"correctness\":{\"checksum\":"
         << bench::json_escape(data.correctness_checksum)
         << ",\"fixture_decodes\":46,\"strict_replay\":true,\"verified\":true}"
         << ",\"host\":{\"architecture\":" << bench::json_escape(data.host.architecture)
         << ",\"cpu_affinity\":" << bench::json_escape(data.host.cpu_affinity)
         << ",\"cpu_governor\":" << bench::json_escape(data.host.cpu_governor)
         << ",\"cpu_model\":" << bench::json_escape(data.host.cpu_model)
         << ",\"kernel\":" << bench::json_escape(data.host.kernel)
         << ",\"limitations\":[";
  for (std::size_t index = 0U; index < data.host.limitations.size(); ++index) {
    if (index != 0U) {
      output << ',';
    }
    output << bench::json_escape(data.host.limitations[index]);
  }
  output << "],\"logical_cpus\":" << data.host.logical_cpus
         << ",\"machine_model\":" << bench::json_escape(data.host.machine_model)
         << ",\"memory_bytes\":" << data.host.memory_bytes
         << ",\"os\":" << bench::json_escape(data.host.os)
         << ",\"physical_cpus\":" << data.host.physical_cpus
         << ",\"turbo_state\":" << bench::json_escape(data.host.turbo_state)
         << "},\"publishable\":false,\"schema_version\":"
         << bench::result_schema_version
         << ",\"timestamp_utc\":" << bench::json_escape(data.timestamp)
         << ",\"warnings\":[";
  for (std::size_t index = 0U; index < data.warnings.size(); ++index) {
    if (index != 0U) {
      output << ',';
    }
    output << bench::json_escape(data.warnings[index]);
  }
  output << "]}\n";
  return output.str();
}

[[nodiscard]] std::string make_csv(const report& data) {
  std::ostringstream output;
  output.imbue(std::locale::classic());
  output << std::setprecision(17);
  output
      << "schema_version,contract_version,timestamp_utc,benchmark_id,operation,pipeline,"
         "workload,workload_sha256,corpus_sha256,bytes_per_round,messages_per_round,"
         "events_per_round,rounds_per_sample,samples,warmup,minimum_time_ms,"
         "median_ns_per_message,p05_ns_per_message,p95_ns_per_message,mad_ns_per_message,"
         "median_ns_per_event,median_bytes_per_second,median_messages_per_second,"
         "median_events_per_second,relative_mad,relative_p95_p05_spread,noisy,implausible,"
         "anti_elision_checksum,feedforge_version,build_type,compiler_id,compiler_version,"
         "compiler_path,config_flags,target_flags,os,kernel,architecture,cpu_model,"
         "machine_model,logical_cpus,physical_cpus,memory_bytes,cpu_affinity,cpu_governor,"
         "turbo_state,correctness_checksum,command\n";
  for (const case_result& result : data.results) {
    const case_definition& definition = *result.definition;
    std::ostringstream checksum;
    checksum << "0x" << std::hex << result.samples.front().checksum;
    output << bench::result_schema_version << ','
           << bench::csv_escape(bench::contract_version) << ','
           << bench::csv_escape(data.timestamp) << ','
           << bench::csv_escape(definition.id) << ','
           << bench::csv_escape(operation_name(definition.operation)) << ','
           << bench::csv_escape(pipeline_name(definition.pipeline)) << ','
           << bench::csv_escape(definition.workload->name) << ','
           << bench::csv_escape(definition.workload_sha256) << ','
           << bench::csv_escape(data.corpus.sha256) << ','
           << definition.bytes_per_round << ',' << definition.messages_per_round << ','
           << definition.events_per_round << ',' << result.rounds_per_sample << ','
           << data.configuration.samples << ',' << data.configuration.warmup << ','
           << data.configuration.minimum_time_ms << ','
           << result.ns_per_message.median << ',' << result.ns_per_message.p05 << ','
           << result.ns_per_message.p95 << ',' << result.ns_per_message.mad << ',';
    if (result.ns_per_event.has_value()) {
      output << result.ns_per_event->median;
    }
    output << ',' << result.bytes_per_second.median << ','
           << result.messages_per_second.median << ',';
    if (result.events_per_second.has_value()) {
      output << result.events_per_second->median;
    }
    output << ',' << result.relative_mad << ','
           << result.relative_p95_p05_spread << ','
           << (result.noisy ? "true" : "false") << ','
           << (result.implausible ? "true" : "false") << ','
           << bench::csv_escape(checksum.str()) << ','
           << bench::csv_escape(feedforge::version_string) << ','
           << bench::csv_escape(bench::build_config::build_type) << ','
           << bench::csv_escape(bench::build_config::compiler_id) << ','
           << bench::csv_escape(bench::build_config::compiler_version) << ','
           << bench::csv_escape(bench::build_config::compiler_path) << ','
           << bench::csv_escape(bench::build_config::config_flags) << ','
           << bench::csv_escape(bench::build_config::target_flags) << ','
           << bench::csv_escape(data.host.os) << ','
           << bench::csv_escape(data.host.kernel) << ','
           << bench::csv_escape(data.host.architecture) << ','
           << bench::csv_escape(data.host.cpu_model) << ','
           << bench::csv_escape(data.host.machine_model) << ','
           << data.host.logical_cpus << ',' << data.host.physical_cpus << ','
           << data.host.memory_bytes << ','
           << bench::csv_escape(data.host.cpu_affinity) << ','
           << bench::csv_escape(data.host.cpu_governor) << ','
           << bench::csv_escape(data.host.turbo_state) << ','
           << bench::csv_escape(data.correctness_checksum) << ','
           << bench::csv_escape(data.command) << '\n';
  }
  return output.str();
}

void print_human(const report& data) {
  std::cout << "FeedForge post-v0.1 benchmark contract "
            << bench::contract_version << '\n'
            << "  corpus sha256: " << data.corpus.sha256 << '\n'
            << "  build: " << bench::build_config::compiler_id << ' '
            << bench::build_config::compiler_version << ", "
            << bench::build_config::build_type << ", "
            << data.host.os << ' ' << data.host.architecture << '\n'
            << "  host: " << data.host.cpu_model << '\n'
            << "  config: samples=" << data.configuration.samples
            << " warmup=" << data.configuration.warmup
            << " min_time_ms=" << data.configuration.minimum_time_ms
            << " batch_floor=" << data.configuration.batch << "\n\n";

  for (const case_result& result : data.results) {
    const double mebi_bytes =
        result.bytes_per_second.median / (1024.0 * 1024.0);
    const double million_messages =
        result.messages_per_second.median / 1'000'000.0;
    std::cout << result.definition->id << '\n'
              << "  median " << std::fixed << std::setprecision(3)
              << result.ns_per_message.median << " ns/message"
              << "  p05 " << result.ns_per_message.p05
              << "  p95 " << result.ns_per_message.p95
              << "  MAD " << result.ns_per_message.mad << '\n'
              << "  " << mebi_bytes << " MiB/s  " << million_messages
              << " Mmessage/s";
    if (result.events_per_second.has_value()) {
      std::cout << "  " << result.events_per_second->median / 1'000'000.0
                << " Mevent/s";
    } else {
      std::cout << "  events n/a (known-unselected skip workload)";
    }
    std::cout << "  rounds/sample=" << result.rounds_per_sample;
    if (result.noisy) {
      std::cout << "  [NOISY]";
    }
    if (result.implausible) {
      std::cout << "  [IMPLAUSIBLE]";
    }
    std::cout << '\n';
  }
  std::cout << "\nDiagnostic only: a single process run is never a publishable "
               "performance claim.\n";
  for (const std::string& warning : data.warnings) {
    std::cout << "warning: " << warning << '\n';
  }
}

[[nodiscard]] report run(const options& configuration, const int argc,
                         char** argv) {
  if (!clock_type::is_steady) {
    throw std::runtime_error("std::chrono::steady_clock is not steady on this platform");
  }

  report result;
  result.configuration = configuration;
  result.arguments.reserve(static_cast<std::size_t>(argc));
  for (int index = 0; index < argc; ++index) {
    result.arguments.emplace_back(argv[index]);
  }
  result.command =
      bench::join_command(std::span<char* const>{argv, static_cast<std::size_t>(argc)});
  result.working_directory = std::filesystem::current_path().string();

  // All file I/O, parsing, hashing, allocation, host discovery, and correctness
  // validation happen before any benchmark clock starts.
  result.corpus = bench::load_corpus(bench::build_config::fixture_directory);
  result.workloads = bench::make_workloads(result.corpus);
  result.cases = make_cases(result.workloads);
  result.correctness_checksum =
      verify_correctness(result.corpus, result.workloads);
  result.host = bench::read_host_manifest();
  result.timer_resolution_ns = timer_resolution_ns();
  result.timestamp = bench::utc_timestamp();

  result.warnings.emplace_back(
      "single process run is diagnostic only; use at least seven independent repeats");
  result.warnings.insert(result.warnings.end(), result.host.limitations.begin(),
                         result.host.limitations.end());
  if (bench::build_config::build_type != "Release") {
    result.warnings.emplace_back(
        "non-Release build does not satisfy the frozen benchmark contract");
  }
  if (result.timer_resolution_ns <= 0.0 ||
      result.timer_resolution_ns > 100'000.0) {
    result.warnings.emplace_back(
        "steady-clock resolution is unavailable or too coarse for this harness");
  }

  result.results.reserve(result.cases.size());
  for (const case_definition& definition : result.cases) {
    result.results.push_back(
        measure_case(definition, result.corpus, configuration));
    if (result.results.back().noisy) {
      result.warnings.emplace_back(definition.id + " was flagged noisy");
    }
    if (result.results.back().implausible) {
      result.warnings.emplace_back(definition.id + " was flagged implausible");
    }
  }
  return result;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const options configuration = parse_options(argc, argv);
    const report results = run(configuration, argc, argv);
    print_human(results);
    if (!configuration.json_path.empty()) {
      bench::atomic_write(configuration.json_path, make_json(results));
      std::cout << "JSON: " << configuration.json_path.string() << '\n';
    }
    if (!configuration.csv_path.empty()) {
      bench::atomic_write(configuration.csv_path, make_csv(results));
      std::cout << "CSV: " << configuration.csv_path.string() << '\n';
    }
    const bool implausible =
        std::any_of(results.results.begin(), results.results.end(),
                    [](const case_result& result) { return result.implausible; });
    return implausible ? 1 : 0;
  } catch (const std::exception& error) {
    std::cerr << "feedforge_benchmark: " << error.what() << '\n';
    return 2;
  }
}
