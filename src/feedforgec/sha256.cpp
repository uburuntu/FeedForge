#include "sha256.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace feedforge::compiler {
namespace {

constexpr std::array<std::uint32_t, 64> round_constants{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU,
    0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U,
    0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U,
    0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U,
    0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
    0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U,
    0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U, 0x1e376c08U,
    0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU,
    0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

class sha256_state {
 public:
  void update(const std::span<const std::byte> input) noexcept {
    for (const std::byte value : input) {
      block_[block_size_] = std::to_integer<std::uint8_t>(value);
      ++block_size_;
      ++total_bytes_;
      if (block_size_ == block_.size()) {
        transform();
        block_size_ = 0U;
      }
    }
  }

  [[nodiscard]] std::array<std::uint8_t, 32> finish() noexcept {
    const std::uint64_t bit_count = total_bytes_ * 8U;
    block_[block_size_] = 0x80U;
    ++block_size_;

    if (block_size_ > 56U) {
      while (block_size_ < block_.size()) {
        block_[block_size_] = 0U;
        ++block_size_;
      }
      transform();
      block_size_ = 0U;
    }
    while (block_size_ < 56U) {
      block_[block_size_] = 0U;
      ++block_size_;
    }
    for (std::size_t index = 0U; index < 8U; ++index) {
      const std::size_t shift = (7U - index) * 8U;
      block_[56U + index] =
          static_cast<std::uint8_t>((bit_count >> shift) & 0xffU);
    }
    transform();

    std::array<std::uint8_t, 32> digest{};
    for (std::size_t word = 0U; word < hash_.size(); ++word) {
      for (std::size_t byte = 0U; byte < 4U; ++byte) {
        const std::size_t shift = (3U - byte) * 8U;
        digest[(word * 4U) + byte] =
            static_cast<std::uint8_t>((hash_[word] >> shift) & 0xffU);
      }
    }
    return digest;
  }

 private:
  void transform() noexcept {
    std::array<std::uint32_t, 64> schedule{};
    for (std::size_t index = 0U; index < 16U; ++index) {
      const std::size_t offset = index * 4U;
      schedule[index] =
          (static_cast<std::uint32_t>(block_[offset]) << 24U) |
          (static_cast<std::uint32_t>(block_[offset + 1U]) << 16U) |
          (static_cast<std::uint32_t>(block_[offset + 2U]) << 8U) |
          static_cast<std::uint32_t>(block_[offset + 3U]);
    }
    for (std::size_t index = 16U; index < schedule.size(); ++index) {
      const std::uint32_t s0 =
          std::rotr(schedule[index - 15U], 7) ^
          std::rotr(schedule[index - 15U], 18) ^
          (schedule[index - 15U] >> 3U);
      const std::uint32_t s1 =
          std::rotr(schedule[index - 2U], 17) ^
          std::rotr(schedule[index - 2U], 19) ^
          (schedule[index - 2U] >> 10U);
      schedule[index] = schedule[index - 16U] + s0 +
                        schedule[index - 7U] + s1;
    }

    std::uint32_t a = hash_[0];
    std::uint32_t b = hash_[1];
    std::uint32_t c = hash_[2];
    std::uint32_t d = hash_[3];
    std::uint32_t e = hash_[4];
    std::uint32_t f = hash_[5];
    std::uint32_t g = hash_[6];
    std::uint32_t h = hash_[7];
    for (std::size_t index = 0U; index < schedule.size(); ++index) {
      const std::uint32_t sum1 =
          std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
      const std::uint32_t choice = (e & f) ^ ((~e) & g);
      const std::uint32_t temporary1 =
          h + sum1 + choice + round_constants[index] + schedule[index];
      const std::uint32_t sum0 =
          std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
      const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
      const std::uint32_t temporary2 = sum0 + majority;

      h = g;
      g = f;
      f = e;
      e = d + temporary1;
      d = c;
      c = b;
      b = a;
      a = temporary1 + temporary2;
    }
    hash_[0] += a;
    hash_[1] += b;
    hash_[2] += c;
    hash_[3] += d;
    hash_[4] += e;
    hash_[5] += f;
    hash_[6] += g;
    hash_[7] += h;
  }

  std::array<std::uint32_t, 8> hash_{
      0x6a09e667U,
      0xbb67ae85U,
      0x3c6ef372U,
      0xa54ff53aU,
      0x510e527fU,
      0x9b05688cU,
      0x1f83d9abU,
      0x5be0cd19U,
  };
  std::array<std::uint8_t, 64> block_{};
  std::size_t block_size_{};
  std::uint64_t total_bytes_{};
};

}  // namespace

std::array<std::uint8_t, 32> sha256(
    const std::span<const std::byte> input) noexcept {
  sha256_state state;
  state.update(input);
  return state.finish();
}

std::string sha256_hex(const std::span<const std::byte> input) {
  constexpr std::string_view digits{"0123456789abcdef"};
  const auto digest = sha256(input);
  std::string result;
  result.reserve(digest.size() * 2U);
  for (const std::uint8_t byte : digest) {
    result.push_back(digits[static_cast<std::size_t>(byte >> 4U)]);
    result.push_back(digits[static_cast<std::size_t>(byte & 0x0fU)]);
  }
  return result;
}

std::string sha256_hex(const std::string_view input) {
  return sha256_hex(std::as_bytes(std::span{input.data(), input.size()}));
}

}  // namespace feedforge::compiler
