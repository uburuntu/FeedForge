#ifndef FEEDFORGE_CONFIG_HPP
#define FEEDFORGE_CONFIG_HPP

#include <bit>
#include <climits>
#include <cstddef>
#include <cstdint>

static_assert(CHAR_BIT == 8, "FeedForge requires eight-bit bytes");
static_assert(sizeof(void*) == 8, "FeedForge requires a 64-bit host");
static_assert(sizeof(std::size_t) <= sizeof(std::uint64_t),
              "FeedForge offsets require size_t to fit in uint64_t");
static_assert(sizeof(std::uint8_t) == 1, "FeedForge requires an 8-bit uint8_t");
static_assert(sizeof(std::uint16_t) == 2, "FeedForge requires a 16-bit uint16_t");
static_assert(sizeof(std::uint32_t) == 4, "FeedForge requires a 32-bit uint32_t");
static_assert(sizeof(std::uint64_t) == 8, "FeedForge requires a 64-bit uint64_t");
static_assert(
    std::endian::native == std::endian::little ||
        std::endian::native == std::endian::big,
    "FeedForge does not support mixed-endian hosts");

#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(clang::nonblocking)
#define FEEDFORGE_NONBLOCKING [[clang::nonblocking]]
#endif
#endif

#ifndef FEEDFORGE_NONBLOCKING
#define FEEDFORGE_NONBLOCKING
#endif

#if defined(_MSC_VER)
#define FEEDFORGE_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define FEEDFORGE_FORCE_INLINE inline __attribute__((always_inline))
#else
#define FEEDFORGE_FORCE_INLINE inline
#endif

#endif  // FEEDFORGE_CONFIG_HPP
