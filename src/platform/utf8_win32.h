#pragma once

#include <windows.h>

#include <cstddef>
#include <string>
#include <string_view>

namespace flux::platform {

inline std::wstring utf8_to_wide(const char* data, size_t length) {
    if (data == nullptr || length == 0) {
        return std::wstring();
    }
    int count = MultiByteToWideChar(CP_UTF8, 0, data, static_cast<int>(length), nullptr, 0);
    if (count <= 0) {
        return std::wstring();
    }
    std::wstring result(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, data, static_cast<int>(length), result.data(), count);
    return result;
}

inline std::wstring utf8_to_wide(std::string_view value) {
    return utf8_to_wide(value.data(), value.size());
}

inline std::string wide_to_utf8(const wchar_t* data, size_t length) {
    if (data == nullptr || length == 0) {
        return std::string();
    }
    int count = WideCharToMultiByte(
        CP_UTF8, 0, data, static_cast<int>(length), nullptr, 0, nullptr, nullptr);
    if (count <= 0) {
        return std::string();
    }
    std::string result(static_cast<size_t>(count), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, data, static_cast<int>(length), result.data(), count, nullptr, nullptr);
    return result;
}

inline std::string wide_to_utf8(const std::wstring& value) {
    return wide_to_utf8(value.data(), value.size());
}

}
