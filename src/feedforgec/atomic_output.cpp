#include "atomic_output.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <random>
#include <string>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace feedforge::compiler {
namespace {

namespace fs = std::filesystem;

constexpr std::size_t temporary_path_attempts = 128U;

[[nodiscard]] diagnostic output_error(const std::string_view destination,
                                      const std::string_view object_path, std::string message) {
  return make_diagnostic("FFIO002", std::string(destination), source_mark{},
                         std::string(object_path), std::move(message));
}

[[nodiscard]] result<void> validate_output_parent(const std::string_view destination,
                                                  const std::string_view object_path) {
  const fs::path path{destination};
  const fs::path parent = path.parent_path().empty() ? fs::path{"."} : path.parent_path();
  std::error_code error;
  if (!fs::is_directory(parent, error) || error) {
    return std::unexpected(output_error(
        destination, object_path, "output parent directory does not exist or is not a directory"));
  }
  return {};
}

[[nodiscard]] std::array<std::uint64_t, 2> make_nonce() {
  static std::atomic_uint64_t sequence{};

  std::random_device entropy;
  std::array<std::uint64_t, 2> nonce{};
  for (auto& value : nonce) {
    for (std::size_t shift = 0U; shift < 64U; shift += 8U) {
      value ^= static_cast<std::uint64_t>(entropy() & 0xffU) << shift;
    }
  }
  nonce[1] ^= sequence.fetch_add(1U, std::memory_order_relaxed);
  return nonce;
}

[[nodiscard]] fs::path temporary_sibling(const fs::path& destination) {
  constexpr std::array<char, 16> hex_digits{'0', '1', '2', '3', '4', '5', '6', '7',
                                            '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

  std::string suffix{".feedforgec.tmp."};
  for (const std::uint64_t value : make_nonce()) {
    for (int shift = 60; shift >= 0; shift -= 4) {
      const auto index = static_cast<std::size_t>((value >> shift) & 0x0fU);
      suffix.push_back(hex_digits[index]);
    }
  }

  fs::path filename = destination.filename();
  filename += fs::path{suffix};
  return destination.parent_path() / filename;
}

#if defined(_WIN32)

class pending_file {
public:
  pending_file(fs::path path, const HANDLE handle) : path_(std::move(path)), handle_(handle) {}

  pending_file(const pending_file&) = delete;
  pending_file& operator=(const pending_file&) = delete;

  pending_file(pending_file&& other) noexcept
      : path_(std::move(other.path_)), handle_(std::exchange(other.handle_, INVALID_HANDLE_VALUE)),
        remove_(std::exchange(other.remove_, false)) {}

  pending_file& operator=(pending_file&&) = delete;

  ~pending_file() {
    if (handle_ != INVALID_HANDLE_VALUE) {
      CloseHandle(handle_);
    }
    if (remove_) {
      std::error_code ignored;
      fs::remove(path_, ignored);
    }
  }

  [[nodiscard]] HANDLE handle() const noexcept { return handle_; }
  [[nodiscard]] const fs::path& path() const noexcept { return path_; }

  [[nodiscard]] bool close() noexcept {
    const HANDLE handle = std::exchange(handle_, INVALID_HANDLE_VALUE);
    return CloseHandle(handle) != 0;
  }

  void release() noexcept { remove_ = false; }

private:
  fs::path path_;
  HANDLE handle_{INVALID_HANDLE_VALUE};
  bool remove_{true};
};

[[nodiscard]] result<pending_file> create_temporary_file(const fs::path& destination,
                                                         const std::string_view destination_text,
                                                         const std::string_view object_path) {
  for (std::size_t attempt = 0U; attempt < temporary_path_attempts; ++attempt) {
    fs::path path = temporary_sibling(destination);
    const HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
      return pending_file{std::move(path), handle};
    }
    const DWORD error = GetLastError();
    if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
      return std::unexpected(
          output_error(destination_text, object_path,
                       "failed to securely create a temporary sibling output file"));
    }
  }
  return std::unexpected(output_error(destination_text, object_path,
                                      "could not reserve a unique temporary sibling output path"));
}

[[nodiscard]] bool write_contents(const HANDLE handle, std::string_view contents) noexcept {
  while (!contents.empty()) {
    const auto request = static_cast<DWORD>(
        std::min<std::size_t>(contents.size(), std::numeric_limits<DWORD>::max()));
    DWORD written = 0U;
    if (WriteFile(handle, contents.data(), request, &written, nullptr) == 0 || written == 0U) {
      return false;
    }
    contents.remove_prefix(written);
  }
  return true;
}

[[nodiscard]] bool replace_destination(const fs::path& temporary,
                                       const fs::path& destination) noexcept {
  return MoveFileExW(temporary.c_str(), destination.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

#else

class pending_file {
public:
  pending_file(fs::path path, const int descriptor)
      : path_(std::move(path)), descriptor_(descriptor) {}

  pending_file(const pending_file&) = delete;
  pending_file& operator=(const pending_file&) = delete;

  pending_file(pending_file&& other) noexcept
      : path_(std::move(other.path_)), descriptor_(std::exchange(other.descriptor_, -1)),
        remove_(std::exchange(other.remove_, false)) {}

  pending_file& operator=(pending_file&&) = delete;

  ~pending_file() {
    if (descriptor_ >= 0) {
      ::close(descriptor_);
    }
    if (remove_) {
      std::error_code ignored;
      fs::remove(path_, ignored);
    }
  }

  [[nodiscard]] int descriptor() const noexcept { return descriptor_; }
  [[nodiscard]] const fs::path& path() const noexcept { return path_; }

  [[nodiscard]] bool close() noexcept {
    const int descriptor = std::exchange(descriptor_, -1);
    return ::close(descriptor) == 0;
  }

  void release() noexcept { remove_ = false; }

private:
  fs::path path_;
  int descriptor_{-1};
  bool remove_{true};
};

[[nodiscard]] result<pending_file> create_temporary_file(const fs::path& destination,
                                                         const std::string_view destination_text,
                                                         const std::string_view object_path) {
  for (std::size_t attempt = 0U; attempt < temporary_path_attempts; ++attempt) {
    fs::path path = temporary_sibling(destination);
    const int descriptor = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
    if (descriptor >= 0) {
      return pending_file{std::move(path), descriptor};
    }
    if (errno != EEXIST) {
      return std::unexpected(
          output_error(destination_text, object_path,
                       "failed to securely create a temporary sibling output file"));
    }
  }
  return std::unexpected(output_error(destination_text, object_path,
                                      "could not reserve a unique temporary sibling output path"));
}

[[nodiscard]] bool write_contents(const int descriptor, std::string_view contents) noexcept {
  while (!contents.empty()) {
    const std::size_t request = std::min<std::size_t>(
        contents.size(), static_cast<std::size_t>(std::numeric_limits<ssize_t>::max()));
    const ssize_t written = ::write(descriptor, contents.data(), request);
    if (written < 0 && errno == EINTR) {
      continue;
    }
    if (written <= 0) {
      return false;
    }
    contents.remove_prefix(static_cast<std::size_t>(written));
  }
  return true;
}

[[nodiscard]] bool flush_file(const int descriptor) noexcept {
  while (::fsync(descriptor) != 0) {
    if (errno != EINTR) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool replace_destination(const fs::path& temporary,
                                       const fs::path& destination) noexcept {
  return ::rename(temporary.c_str(), destination.c_str()) == 0;
}

#endif

} // namespace

result<void> write_file_atomically(const std::string_view destination,
                                   const std::string_view contents,
                                   const std::string_view object_path) {
  if (auto parent = validate_output_parent(destination, object_path); !parent) {
    return std::unexpected(std::move(parent.error()));
  }

  const fs::path destination_path{destination};
  auto file = create_temporary_file(destination_path, destination, object_path);
  if (!file) {
    return std::unexpected(std::move(file.error()));
  }

#if defined(_WIN32)
  if (!write_contents(file->handle(), contents)) {
#else
  if (!write_contents(file->descriptor(), contents)) {
#endif
    return std::unexpected(
        output_error(destination, object_path, "failed while writing temporary output"));
  }

#if defined(_WIN32)
  if (FlushFileBuffers(file->handle()) == 0) {
#else
  if (!flush_file(file->descriptor())) {
#endif
    return std::unexpected(
        output_error(destination, object_path, "failed while flushing temporary output"));
  }

  if (!file->close()) {
    return std::unexpected(
        output_error(destination, object_path, "failed while closing temporary output"));
  }
  if (!replace_destination(file->path(), destination_path)) {
    return std::unexpected(
        output_error(destination, object_path, "failed to atomically replace output file"));
  }
  file->release();
  return {};
}

} // namespace feedforge::compiler
