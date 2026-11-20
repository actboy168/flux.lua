#pragma once

#include <windows.h>

namespace flux::platform {

inline DWORD build_number() {
    static DWORD build = [] {
        OSVERSIONINFOW info = {};
        info.dwOSVersionInfoSize = sizeof(info);
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (ntdll != nullptr) {
            using rtl_get_version_fn = LONG(WINAPI*)(OSVERSIONINFOW*);
            auto fn = reinterpret_cast<rtl_get_version_fn>(
                reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion")));
            if (fn != nullptr && fn(&info) == 0) {
                return info.dwBuildNumber;
            }
        }
        return DWORD{0};
    }();
    return build;
}

inline bool supports_system_backdrop() {
    return build_number() >= 22621;
}

}
