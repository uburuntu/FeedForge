#include "benchmark_support.hpp"

#include <atomic>
#include <cstddef>

namespace feedforge::benchmark {
namespace {

const void* volatile escaped_address{};
volatile std::size_t escaped_size{};

}  // namespace

#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void opaque_escape(const void* address, const std::size_t size) noexcept {
  std::atomic_signal_fence(std::memory_order_seq_cst);
  escaped_address = address;
  escaped_size = size;
  std::atomic_signal_fence(std::memory_order_seq_cst);
}

}  // namespace feedforge::benchmark
