#include "synthetic_pipeline.hpp"

#include <array>
#include <cstddef>

namespace generated = feedforge::generated::test_projection;

struct first_sink {
  feedforge::flow operator()(const generated::add_order&) noexcept {
    return feedforge::flow::continue_;
  }

  feedforge::flow operator()(const generated::order_update&) noexcept {
    return feedforge::flow::continue_;
  }
};

struct second_sink {
  feedforge::flow operator()(const generated::add_order&) noexcept {
    return feedforge::flow::continue_;
  }

  feedforge::flow operator()(const generated::order_update&) noexcept {
    return feedforge::flow::continue_;
  }
};

struct ambiguous_sink : first_sink, second_sink {
  using first_sink::operator();
  using second_sink::operator();
};

int main() {
  generated::decoder decoder;
  ambiguous_sink sink;
  const std::array payload{
      std::byte{'U'},
      std::byte{0x00},
      std::byte{0x01},
      std::byte{'A'},
  };
  static_cast<void>(decoder.decode_one(payload, sink));
}
