#include <cstddef>
#include <exception>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "compiler_limits.hpp"
#include "diagnostics.hpp"
#include "emit_cpp.hpp"
#include "feedforge/version.hpp"
#include "ir.hpp"
#include "lower.hpp"
#include "model.hpp"
#include "parse_toml.hpp"

namespace {

namespace fs = std::filesystem;
namespace compiler = feedforge::compiler;

enum class command {
  validate,
  compile,
  dump_ir,
};

struct options {
  command action{};
  std::string schema;
  std::string pipeline;
  std::string output;
};

[[nodiscard]] compiler::diagnostic usage_error(std::string message) {
  return compiler::make_diagnostic("FFCLI001", {}, compiler::source_mark{},
                                   "cli", std::move(message));
}

void print_help(std::ostream& output) {
  output << "FeedForge compiler " << feedforge::version_string << "\n\n"
         << "Usage:\n"
         << "  feedforgec validate --schema <file> [--pipeline <file>]\n"
         << "  feedforgec compile --schema <file> --pipeline <file> "
            "--output <file>\n"
         << "  feedforgec dump-ir --schema <file> --pipeline <file> "
            "--output <file>\n"
         << "  feedforgec --version\n"
         << "  feedforgec --help\n";
}

[[nodiscard]] compiler::result<options> parse_arguments(const int argc,
                                                        char** argv) {
  if (argc < 2) {
    return std::unexpected(usage_error("missing command"));
  }

  options parsed{};
  const std::string_view action{argv[1]};
  if (action == "validate") {
    parsed.action = command::validate;
  } else if (action == "compile") {
    parsed.action = command::compile;
  } else if (action == "dump-ir") {
    parsed.action = command::dump_ir;
  } else {
    return std::unexpected(
        usage_error("unknown command '" + std::string(action) + "'"));
  }

  const auto read_path =
      [&](int& index, const std::string_view flag,
          std::string& destination) -> compiler::result<void> {
    if (!destination.empty()) {
      return std::unexpected(
          usage_error("duplicate option '" + std::string(flag) + "'"));
    }
    ++index;
    if (index >= argc) {
      return std::unexpected(
          usage_error("option '" + std::string(flag) + "' requires a path"));
    }
    destination = argv[index];
    if (destination.empty()) {
      return std::unexpected(
          usage_error("option '" + std::string(flag) + "' requires a path"));
    }
    return {};
  };

  for (int index = 2; index < argc; ++index) {
    const std::string_view flag{argv[index]};
    compiler::result<void> parsed_option{};
    if (flag == "--schema") {
      parsed_option = read_path(index, flag, parsed.schema);
    } else if (flag == "--pipeline") {
      parsed_option = read_path(index, flag, parsed.pipeline);
    } else if (flag == "--output") {
      parsed_option = read_path(index, flag, parsed.output);
    } else {
      return std::unexpected(
          usage_error("unknown option '" + std::string(flag) + "'"));
    }
    if (!parsed_option) {
      return std::unexpected(std::move(parsed_option.error()));
    }
  }

  if (parsed.schema.empty()) {
    return std::unexpected(usage_error("--schema is required"));
  }
  if (parsed.action == command::validate) {
    if (!parsed.output.empty()) {
      return std::unexpected(usage_error("validate does not accept --output"));
    }
  } else if (parsed.pipeline.empty() || parsed.output.empty()) {
    return std::unexpected(usage_error(std::string(action) + " requires --pipeline and --output"));
  }
  return parsed;
}

[[nodiscard]] compiler::result<void> validate_output_parent(
    const std::string_view destination, const std::string_view object_path) {
  const fs::path path{destination};
  const fs::path parent =
      path.parent_path().empty() ? fs::path{"."} : path.parent_path();
  std::error_code error;
  if (!fs::is_directory(parent, error) || error) {
    return std::unexpected(compiler::make_diagnostic(
        "FFIO002", std::string(destination), compiler::source_mark{},
        std::string(object_path),
        "output parent directory does not exist or is not a directory"));
  }
  return {};
}

[[nodiscard]] bool same_filesystem_location(const std::string_view left,
                                            const std::string_view right) {
  if (left.empty() || right.empty()) {
    return false;
  }
  const fs::path left_path{left};
  const fs::path right_path{right};
  std::error_code error;
  if (fs::exists(left_path, error) && !error &&
      fs::exists(right_path, error) && !error &&
      fs::equivalent(left_path, right_path, error) && !error) {
    return true;
  }
  error.clear();
  const fs::path absolute_left = fs::absolute(left_path, error);
  if (error) {
    return left_path.lexically_normal() == right_path.lexically_normal();
  }
  const fs::path absolute_right = fs::absolute(right_path, error);
  if (error) {
    return left_path.lexically_normal() == right_path.lexically_normal();
  }
  return absolute_left.lexically_normal() == absolute_right.lexically_normal();
}

[[nodiscard]] compiler::result<void> write_atomically(
    const std::string_view destination, const std::string_view contents,
    const std::string_view object_path) {
  if (auto parent = validate_output_parent(destination, object_path); !parent) {
    return std::unexpected(std::move(parent.error()));
  }

  const fs::path destination_path{destination};
  fs::path temporary_path;
  std::error_code error;
  for (std::size_t attempt = 0U; attempt < 1024U; ++attempt) {
    temporary_path =
        destination_path.parent_path() /
        (destination_path.filename().string() + ".feedforgec.tmp." +
         std::to_string(attempt));
    if (!fs::exists(temporary_path, error)) {
      break;
    }
    if (error) {
      return std::unexpected(compiler::make_diagnostic(
          "FFIO002", std::string(destination), compiler::source_mark{},
          std::string(object_path), "failed to inspect temporary output path"));
    }
    temporary_path.clear();
  }
  if (temporary_path.empty()) {
    return std::unexpected(compiler::make_diagnostic(
        "FFIO002", std::string(destination), compiler::source_mark{},
        std::string(object_path),
        "could not reserve a temporary sibling output path"));
  }

  {
    std::ofstream output{temporary_path,
                         std::ios::binary | std::ios::out | std::ios::trunc};
    if (!output) {
      output.close();
      std::error_code ignored;
      fs::remove(temporary_path, ignored);
      return std::unexpected(compiler::make_diagnostic(
          "FFIO002", std::string(destination), compiler::source_mark{},
          std::string(object_path), "failed to create temporary output file"));
    }
    output.write(contents.data(),
                 static_cast<std::streamsize>(contents.size()));
    output.flush();
    if (!output) {
      output.close();
      std::error_code ignored;
      fs::remove(temporary_path, ignored);
      return std::unexpected(compiler::make_diagnostic(
          "FFIO002", std::string(destination), compiler::source_mark{},
          std::string(object_path), "failed while writing temporary output"));
    }
    output.close();
    if (!output) {
      std::error_code ignored;
      fs::remove(temporary_path, ignored);
      return std::unexpected(compiler::make_diagnostic(
          "FFIO002", std::string(destination), compiler::source_mark{},
          std::string(object_path), "failed while closing temporary output"));
    }
  }

  fs::rename(temporary_path, destination_path, error);
  if (error) {
    std::error_code ignored;
    fs::remove(temporary_path, ignored);
    return std::unexpected(compiler::make_diagnostic(
        "FFIO002", std::string(destination), compiler::source_mark{},
        std::string(object_path), "failed to atomically replace output file"));
  }
  return {};
}

int report(const compiler::diagnostic& problem, const int exit_code) {
  std::cerr << compiler::format_diagnostic(problem) << '\n';
  return exit_code;
}

int run(const options& parsed) {
  auto schema_source = compiler::parse_schema_file(parsed.schema);
  if (!schema_source) {
    const int exit_code = schema_source.error().code == "FFIO001" ? 3 : 2;
    return report(schema_source.error(), exit_code);
  }
  auto schema = compiler::validate_schema(*schema_source);
  if (!schema) {
    return report(schema.error(), 2);
  }
  if (parsed.pipeline.empty()) {
    return 0;
  }

  auto pipeline_source = compiler::parse_pipeline_file(parsed.pipeline);
  if (!pipeline_source) {
    const int exit_code = pipeline_source.error().code == "FFIO001" ? 3 : 2;
    return report(pipeline_source.error(), exit_code);
  }
  auto pipeline = compiler::validate_pipeline(*pipeline_source, *schema);
  if (!pipeline) {
    return report(pipeline.error(), 2);
  }

  const compiler::ffir_v1 ir = compiler::lower_to_ffir(
      *schema, *pipeline, feedforge::version_string);
  if (parsed.action == command::validate) {
    return 0;
  }

  if (same_filesystem_location(parsed.output, parsed.schema) ||
      same_filesystem_location(parsed.output, parsed.pipeline)) {
    return report(compiler::make_diagnostic(
                      "FFCLI002", parsed.output, compiler::source_mark{},
                      "output",
                      "--output must not name the schema or pipeline path"),
                  2);
  }

  if (auto parent = validate_output_parent(parsed.output, "output"); !parent) {
    return report(parent.error(), 3);
  }

  std::string rendered;
  if (parsed.action == command::compile) {
    auto rendered_cpp = compiler::emit_cpp(ir);
    if (!rendered_cpp) {
      compiler::diagnostic problem = std::move(rendered_cpp.error());
      problem.path = compiler::normalise_source_path(parsed.output);
      return report(problem, 2);
    }
    rendered = std::move(*rendered_cpp);
  } else {
    rendered = compiler::canonical_json(ir);
    if (rendered.size() > compiler::limits::rendered_bytes) {
      return report(
          compiler::make_diagnostic("FFLIMIT001", parsed.output, compiler::source_mark{}, "output",
                                    "FFIR JSON exceeds the 16777216-byte rendered output limit"),
          2);
    }
  }
  if (auto written = write_atomically(parsed.output, rendered, "output"); !written) {
    return report(written.error(), 3);
  }
  return 0;
}

}  // namespace

int main_impl(const int argc, char** argv) {
  if (argc == 2 && std::string_view{argv[1]} == "--help") {
    print_help(std::cout);
    return 0;
  }
  if (argc == 2 && std::string_view{argv[1]} == "--version") {
    std::cout << "feedforgec " << feedforge::version_string << '\n';
    return 0;
  }

  auto parsed = parse_arguments(argc, argv);
  if (!parsed) {
    print_help(std::cerr);
    return report(parsed.error(), 2);
  }
  return run(*parsed);
}

int main(const int argc, char** argv) {
  try {
    return main_impl(argc, argv);
  } catch (const std::exception&) {
    return report(compiler::make_diagnostic("FFINTERNAL001", {}, compiler::source_mark{},
                                            "compiler", "unexpected internal compiler failure"),
                  4);
  } catch (...) {
    return report(compiler::make_diagnostic("FFINTERNAL001", {}, compiler::source_mark{},
                                            "compiler", "unexpected internal compiler failure"),
                  4);
  }
}
