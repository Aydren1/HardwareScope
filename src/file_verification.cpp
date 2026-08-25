#include "hardwarescope/file_verification.hpp"

#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <cctype>
#include <cstdio>
#include <vector>

namespace hardwarescope {
namespace {

struct Algorithm final {
    BCRYPT_ALG_HANDLE value{};
    ~Algorithm() { if (value != nullptr) BCryptCloseAlgorithmProvider(value, 0U); }
};

struct Hash final {
    BCRYPT_HASH_HANDLE value{};
    ~Hash() { if (value != nullptr) BCryptDestroyHash(value); }
};

} // namespace

bool VerifyFileSha256(const std::filesystem::path& path, const std::uint64_t expected_size, const std::string_view expected_sha256) noexcept {
    if (expected_sha256.size() != 64U) return false;
    try {
        std::error_code error;
        if (!std::filesystem::is_regular_file(path, error) || error || std::filesystem::file_size(path, error) != expected_size || error) return false;
        FILE* stream{};
        if (_wfopen_s(&stream, path.c_str(), L"rb") != 0 || stream == nullptr) return false;
        struct FileCloser final { FILE* value{}; ~FileCloser() { if (value != nullptr) fclose(value); } } file{stream};

        Algorithm algorithm{};
        if (BCryptOpenAlgorithmProvider(&algorithm.value, BCRYPT_SHA256_ALGORITHM, nullptr, 0U) < 0) return false;
        DWORD object_size{};
        DWORD copied{};
        if (BCryptGetProperty(algorithm.value, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &copied, 0U) < 0) return false;
        std::vector<UCHAR> object(object_size);
        Hash hash{};
        if (BCryptCreateHash(algorithm.value, &hash.value, object.data(), static_cast<ULONG>(object.size()), nullptr, 0U, 0U) < 0) return false;
        std::array<UCHAR, 64U * 1024U> buffer{};
        for (;;) {
            const auto read = fread(buffer.data(), 1U, buffer.size(), stream);
            if (read > 0U && BCryptHashData(hash.value, buffer.data(), static_cast<ULONG>(read), 0U) < 0) return false;
            if (read < buffer.size()) {
                if (ferror(stream) != 0) return false;
                break;
            }
        }
        std::array<UCHAR, 32U> digest{};
        if (BCryptFinishHash(hash.value, digest.data(), static_cast<ULONG>(digest.size()), 0U) < 0) return false;
        constexpr char hex[] = "0123456789ABCDEF";
        std::array<char, 64U> actual{};
        for (std::size_t index = 0U; index < digest.size(); ++index) {
            actual[index * 2U] = hex[digest[index] >> 4U];
            actual[index * 2U + 1U] = hex[digest[index] & 0x0FU];
        }
        for (std::size_t index = 0U; index < actual.size(); ++index) {
            if (actual[index] != static_cast<char>(std::toupper(static_cast<unsigned char>(expected_sha256[index])))) return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace hardwarescope
