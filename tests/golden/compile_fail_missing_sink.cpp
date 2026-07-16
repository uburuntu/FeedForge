#include "synthetic_pipeline.hpp"

#include <array>
#include <cstddef>

namespace generated = feedforge::generated::test_projection;

struct missing_sink {
  feedforge::flow operator()(const generated::add_order&) noexcept {
    return feedforge::flow::continue_;
  }
};

int main() {
  generated::decoder decoder;
  missing_sink sink;
  const std::array payload{
      std::byte{'U'}, std::byte{0x00}, std::byte{0x01}, std::byte{'A'},
  };
  static_cast<void>(decoder.decode_one(payload, sink));
}
