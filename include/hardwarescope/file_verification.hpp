#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace hardwarescope {

[[nodiscard]] bool VerifyFileSha256(const std::filesystem::path& path, std::uint64_t expected_size, std::string_view expected_sha256) noexcept;

} // namespace hardwarescope
