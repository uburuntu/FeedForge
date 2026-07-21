#include <string>

#include "compiler_limits.hpp"
#include "validation_test_support.hpp"

namespace {

namespace compiler = feedforge::compiler;
namespace limits = feedforge::compiler::limits;
namespace support = feedforge::compiler::test;

void test_source_and_document_limits(support::suite& tests) {
  const std::string oversized_source(limits::source_bytes + 1U, 'x');
  tests.expect_error(compiler::parse_schema_toml(oversized_source, "oversized.toml"), "FFLIMIT001",
                     "schema", "1048576-byte", "oversized schema source");

  std::string excessive_nodes = "values = [";
  for (std::size_t index = 0U; index < limits::toml_nodes; ++index) {
    excessive_nodes += "0,";
  }
  excessive_nodes += "0]\n";
  tests.expect_error(compiler::parse_schema_toml(excessive_nodes, "nodes.toml"), "FFLIMIT001",
                     "schema", "32768-node", "excessive TOML nodes");

  const std::string excessive_description(limits::documentation_bytes + 1U, 'x');
  const std::string documented = support::replace_once(
      tests, std::string{support::valid_schema_toml},
      "description = \"Focused validation fixture\"",
      "description = \"" + excessive_description + "\"", "oversized description fixture");
  tests.expect_error(compiler::parse_schema_toml(documented, "description.toml"), "FFLIMIT001",
                     "schema.description", "4096-byte", "oversized documentation string");
}

void test_schema_limits(support::suite& tests) {
  const compiler::schema_source base = support::minimal_schema();
  const std::string long_identifier(limits::identifier_bytes + 1U, 'a');
  tests.check(!compiler::is_valid_source_name(long_identifier),
              "lexical validation rejects oversized identifiers");
  support::expect_schema_error(
      tests, base, [&](auto& source) { source.name = long_identifier; }, "FFLIMIT001",
      "schema.name", "128-byte", "oversized schema identifier");

  support::expect_schema_error(
      tests, base,
      [](auto& source) {
        source.types.assign(limits::user_types + 1U,
                            compiler::type_source{support::test_mark(), "custom", "uint", 1,
                                                  "raw_unsigned", std::nullopt});
      },
      "FFLIMIT001", "schema.types", "256-type", "too many user types");

  support::expect_schema_error(
      tests, base,
      [](auto& source) { source.messages.assign(limits::messages + 1U, source.messages.front()); },
      "FFLIMIT001", "schema.messages", "94-message", "too many messages");

  support::expect_schema_error(
      tests, base,
      [](auto& source) {
        source.messages.front().fields.assign(limits::fields_per_message + 1U,
                                              source.messages.front().fields.front());
      },
      "FFLIMIT001", ".fields", "1024-field", "too many message fields");

  support::expect_schema_error(
      tests, base,
      [](auto& source) {
        source.messages.front().fields.back().allowed.assign(limits::allowed_values_per_field + 1U,
                                                             "X");
      },
      "FFLIMIT001", ".allowed", "256-value", "too many allowed values");
}

void test_pipeline_limits(support::suite& tests) {
  const auto schema = compiler::validate_schema(support::minimal_schema());
  tests.check(schema.has_value(), "pipeline limit schema resolves");
  if (!schema) {
    return;
  }
  const compiler::pipeline_source base = support::minimal_pipeline();
  support::expect_pipeline_error(
      tests, base, *schema,
      [](auto& source) {
        source.cpp_namespace = std::string(limits::identifier_bytes + 1U, 'a');
      },
      "FFLIMIT001", "pipeline.namespace", "128-byte", "oversized namespace component");

  support::expect_pipeline_error(
      tests, base, *schema,
      [](auto& source) {
        source.cpp_namespace = "a::a::a::a::a::a::a::a::a::a::a::a::a::a::a::a::a";
      },
      "FFLIMIT001", "pipeline.namespace", "16-component", "too many namespace components");

  support::expect_pipeline_error(
      tests, base, *schema,
      [](auto& source) {
        source.projections.assign(limits::projections + 1U, source.projections.front());
      },
      "FFLIMIT001", "pipeline.emit", "94-projection", "too many projections");

  support::expect_pipeline_error(
      tests, base, *schema,
      [](auto& source) {
        source.projections.front().fields.assign(limits::fields_per_projection + 1U, "payload");
      },
      "FFLIMIT001", ".fields", "1024-field", "too many projected fields");
}

} // namespace

int main() {
  support::suite tests;
  test_source_and_document_limits(tests);
  test_schema_limits(tests);
  test_pipeline_limits(tests);
  return tests.finish("compiler limits");
}
