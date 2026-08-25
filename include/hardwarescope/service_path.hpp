#pragma once

#include <string>
#include <string_view>

namespace hardwarescope {

[[nodiscard]] inline std::wstring QuoteServiceBinaryPath(const std::wstring_view path) {
    std::wstring result;
    result.reserve(path.size() + 2U);
    result.push_back(L'"');
    result.append(path);
    result.push_back(L'"');
    return result;
}

} // namespace hardwarescope
