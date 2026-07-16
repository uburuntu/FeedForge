#include "diagnostics.hpp"

#include <sstream>
#include <utility>

namespace feedforge::compiler {

std::string normalise_source_path(const std::string_view path) {
  std::string result{path};
  for (char& character : result) {
    if (character == '\\') {
      character = '/';
    }
  }
  return result;
}

std::string format_diagnostic(const diagnostic& problem) {
  std::ostringstream output;
  output << problem.code;
  if (!problem.path.empty()) {
    output << ' ' << problem.path;
    if (problem.position) {
      output << ':' << problem.position->line << ':'
             << problem.position->column;
    }
  }
  output << ':';
  if (!problem.object_path.empty()) {
    output << ' ' << problem.object_path << ':';
  }
  output << ' ' << problem.message;
  if (problem.hint && !problem.hint->empty()) {
    output << "\nhint: " << *problem.hint;
  }
  return output.str();
}

diagnostic make_diagnostic(std::string code, std::string path,
                           const source_mark& mark, std::string object_path,
                           std::string message,
                           std::optional<std::string> hint) {
  return {
      .code = std::move(code),
      .path = normalise_source_path(path),
      .position = mark.position,
      .object_path = std::move(object_path),
      .message = std::move(message),
      .hint = std::move(hint),
  };
}

}  // namespace feedforge::compiler
