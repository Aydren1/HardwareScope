#include "hardwarescope/update_client.hpp"

#include <iostream>

int main() {
    const hardwarescope::SemanticVersion installed{
        HARDWARESCOPE_VERSION_MAJOR,
        HARDWARESCOPE_VERSION_MINOR,
        HARDWARESCOPE_VERSION_PATCH};
    const auto result = hardwarescope::CheckStableUpdate(installed);
    if (result.status == hardwarescope::UpdateCheckStatus::failed) {
        std::cerr << "FAIL: stable update manifest could not be downloaded or verified; error=" << result.system_error << '\n';
        return 1;
    }
    std::cout << "OK: verified stable manifest version "
              << result.manifest.version.major << '.'
              << result.manifest.version.minor << '.'
              << result.manifest.version.patch
              << (result.status == hardwarescope::UpdateCheckStatus::available ? " is available\n" : " does not supersede this build\n");
    return 0;
}
