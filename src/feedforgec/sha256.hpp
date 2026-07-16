#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace feedforge::compiler {

[[nodiscard]] std::array<std::uint8_t, 32> sha256(
    std::span<const std::byte> input) noexcept;
[[nodiscard]] std::string sha256_hex(std::span<const std::byte> input);
[[nodiscard]] std::string sha256_hex(std::string_view input);

}  // namespace feedforge::compiler
