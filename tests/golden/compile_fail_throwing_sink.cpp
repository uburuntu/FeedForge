#include "synthetic_pipeline.hpp"

#include <array>
#include <cstddef>

namespace generated = feedforge::generated::test_projection;

struct throwing_sink {
  feedforge::flow operator()(const generated::add_order&) noexcept {
    return feedforge::flow::continue_;
  }

  feedforge::flow operator()(const generated::order_update&) {
    return feedforge::flow::continue_;
  }
};

int main() {
  generated::decoder decoder;
  throwing_sink sink;
  const std::array payload{
      std::byte{'U'}, std::byte{0x00}, std::byte{0x01}, std::byte{'A'},
  };
  static_cast<void>(decoder.decode_one(payload, sink));
}
