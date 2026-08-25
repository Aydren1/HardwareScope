#include "hardwarescope/update_client.hpp"

#include "hardwarescope/file_verification.hpp"

#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>

#include <array>
#include <charconv>
#include <fstream>
#include <string>
#include <vector>

namespace hardwarescope {
namespace {

// Read the repository file directly so update checks do not depend on GitHub
// Pages being enabled or having completed a separate deployment.
constexpr wchar_t kManifestUrl[] = L"https://raw.githubusercontent.com/Aydren1/HardwareScope/main/updates/latest.json";

struct InternetHandle final {
    HINTERNET value{};
    ~InternetHandle() { if (value != nullptr) WinHttpCloseHandle(value); }
};

bool OpenUrl(const std::wstring_view url, InternetHandle& session, InternetHandle& connection, InternetHandle& request) noexcept {
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.data(), static_cast<DWORD>(url.size()), 0U, &components) || components.nScheme != INTERNET_SCHEME_HTTPS) return false;
    std::wstring host{components.lpszHostName, components.dwHostNameLength};
    std::wstring path{components.lpszUrlPath, components.dwUrlPathLength};
    if (components.dwExtraInfoLength > 0U) path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    session.value = WinHttpOpen(L"HardwareScope/2.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0U);
    if (session.value == nullptr) return false;
    static_cast<void>(WinHttpSetTimeouts(session.value, 5'000, 5'000, 10'000, 15'000));
    connection.value = WinHttpConnect(session.value, host.c_str(), components.nPort, 0U);
    if (connection.value == nullptr) return false;
    request.value = WinHttpOpenRequest(connection.value, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (request.value == nullptr) return false;
    if (!WinHttpSendRequest(request.value, WINHTTP_NO_ADDITIONAL_HEADERS, 0U, WINHTTP_NO_REQUEST_DATA, 0U, 0U, 0U)
        || !WinHttpReceiveResponse(request.value, nullptr)) return false;
    DWORD status{};
    DWORD status_size = sizeof(status);
    return WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX)
        && status == HTTP_STATUS_OK;
}

bool DownloadMemory(const std::wstring_view url, const std::size_t maximum, std::string& destination) noexcept {
    try {
        InternetHandle session{};
        InternetHandle connection{};
        InternetHandle request{};
        if (!OpenUrl(url, session, connection, request)) return false;
        destination.clear();
        std::array<char, 16U * 1024U> buffer{};
        for (;;) {
            DWORD read{};
            if (!WinHttpReadData(request.value, buffer.data(), static_cast<DWORD>(buffer.size()), &read)) return false;
            if (read == 0U) break;
            if (destination.size() + read > maximum) return false;
            destination.append(buffer.data(), read);
        }
        return true;
    } catch (...) {
        return false;
    }
}

std::optional<std::filesystem::path> UpdateDirectory() noexcept {
    PWSTR local_app_data{};
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &local_app_data))) return std::nullopt;
    std::filesystem::path path{local_app_data};
    CoTaskMemFree(local_app_data);
    path /= L"HardwareScope";
    path /= L"Updates";
    std::error_code error;
    std::filesystem::create_directories(path, error);
    if (error) return std::nullopt;
    return path;
}

std::wstring Quote(const std::wstring_view argument) {
    std::wstring result{L"\""};
    for (const auto character : argument) {
        if (character == L'\"') result += L'\\';
        result += character;
    }
    result += L'\"';
    return result;
}

} // namespace

UpdateCheckResult CheckStableUpdate(const SemanticVersion& installed_version) noexcept {
    std::string json;
    if (!DownloadMemory(kManifestUrl, 64U * 1024U, json)) return UpdateCheckResult{UpdateCheckStatus::failed, {}, GetLastError()};
    const auto manifest = ParseUpdateManifest(json);
    if (!manifest) return UpdateCheckResult{UpdateCheckStatus::failed, {}, ERROR_INVALID_DATA};
    return UpdateCheckResult{manifest->version > installed_version ? UpdateCheckStatus::available : UpdateCheckStatus::current, *manifest, ERROR_SUCCESS};
}

std::optional<std::filesystem::path> DownloadVerifiedInstaller(const UpdateManifest& manifest) noexcept {
    if (!IsTrustedInstallerUrl(manifest.installer_url)) return std::nullopt;
    try {
        const auto directory = UpdateDirectory();
        if (!directory) return std::nullopt;
        const auto name = L"HardwareScope-Setup-" + std::to_wstring(manifest.version.major) + L"." + std::to_wstring(manifest.version.minor) + L"." + std::to_wstring(manifest.version.patch) + L"-x64.exe";
        const auto completed = *directory / name;
        auto partial = completed;
        partial += L".partial";
        if (VerifyFileSha256(completed, manifest.installer_size, manifest.sha256)) return completed;
        std::error_code error;
        std::filesystem::remove(partial, error);
        struct PartialCleanup final {
            std::filesystem::path path;
            bool keep{};
            ~PartialCleanup() {
                if (keep) return;
                std::error_code ignored;
                std::filesystem::remove(path, ignored);
            }
        } cleanup{partial, false};

        InternetHandle session{};
        InternetHandle connection{};
        InternetHandle request{};
        if (!OpenUrl(manifest.installer_url, session, connection, request)) return std::nullopt;
        std::ofstream stream{partial, std::ios::binary | std::ios::trunc};
        if (!stream) return std::nullopt;
        std::uint64_t received{};
        std::array<char, 64U * 1024U> buffer{};
        for (;;) {
            DWORD read{};
            if (!WinHttpReadData(request.value, buffer.data(), static_cast<DWORD>(buffer.size()), &read)) return std::nullopt;
            if (read == 0U) break;
            received += read;
            if (received > manifest.installer_size) return std::nullopt;
            stream.write(buffer.data(), read);
            if (!stream) return std::nullopt;
        }
        stream.close();
        if (!VerifyFileSha256(partial, manifest.installer_size, manifest.sha256)) {
            return std::nullopt;
        }
        if (!MoveFileExW(partial.c_str(), completed.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return std::nullopt;
        cleanup.keep = true;
        return completed;
    } catch (...) {
        return std::nullopt;
    }
}

bool LaunchUpdateHandoff(
    const std::filesystem::path& updater,
    const std::filesystem::path& installer,
    const UpdateManifest& manifest,
    const std::uint32_t application_process_id,
    const std::filesystem::path& application_to_relaunch) noexcept {
    try {
        const auto staged_updater = installer.parent_path() / L"HardwareScopeUpdateHandoff.exe";
        std::error_code copy_error;
        std::filesystem::copy_file(updater, staged_updater, std::filesystem::copy_options::overwrite_existing, copy_error);
        if (copy_error) return false;
        std::wstring command = Quote(staged_updater.native());
        command += L" --apply ";
        command += Quote(installer.native());
        command += L" --size ";
        command += std::to_wstring(manifest.installer_size);
        command += L" --sha256 ";
        command += Quote(std::wstring{manifest.sha256.begin(), manifest.sha256.end()});
        command += L" --wait-pid ";
        command += std::to_wstring(application_process_id);
        command += L" --relaunch ";
        command += Quote(application_to_relaunch.native());
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, staged_updater.parent_path().c_str(), &startup, &process)) return false;
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace hardwarescope
