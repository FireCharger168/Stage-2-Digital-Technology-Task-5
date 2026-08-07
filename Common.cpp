// Common.cpp
#include "Common.h"
#include <cwctype>
#include <algorithm>

ULONGLONG SystemTimeToTicks(const SYSTEMTIME& st) {
    FILETIME ft{};
    SystemTimeToFileTime(&st, &ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli.QuadPart;
}

SYSTEMTIME NowLocalSystemTime() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    return st;
}

std::wstring FormatDate(const SYSTEMTIME& st) {
    wchar_t buf[32];
    swprintf_s(buf, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
    return buf;
}

std::wstring FormatDateTime(const SYSTEMTIME& st) {
    wchar_t buf[32];
    swprintf_s(buf, L"%04d-%02d-%02d %02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    return buf;
}

std::wstring GetWindowTextStr(HWND h) {
    int len = GetWindowTextLengthW(h);
    if (len <= 0) return L"";
    std::wstring s(len, L'\0');
    GetWindowTextW(h, &s[0], len + 1);
    return s;
}

bool ContainsCI(const std::wstring& haystack, const std::wstring& needle) {
    if (needle.empty()) return true;
    std::wstring h = haystack;
    std::wstring n = needle;
    std::transform(h.begin(), h.end(), h.begin(), ::towlower);
    std::transform(n.begin(), n.end(), n.begin(), ::towlower);
    return h.find(n) != std::wstring::npos;
}

std::wstring EscapeForStorage(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size());
    for (wchar_t c : s) {
        if (c == L'\\') out += L"\\\\";
        else if (c == L'|') out += L"\\p";
        else if (c == L'\n') out += L"\\n";
        else if (c == L'\r') continue;
        else out += c;
    }
    return out;
}

std::wstring UnescapeFromStorage(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == L'\\' && i + 1 < s.size()) {
            wchar_t next = s[i + 1];
            if (next == L'\\') { out += L'\\'; i++; }
            else if (next == L'p') { out += L'|'; i++; }
            else if (next == L'n') { out += L'\n'; i++; }
            else out += s[i];
        } else {
            out += s[i];
        }
    }
    return out;
}

std::vector<std::wstring> SplitFields(const std::wstring& line, wchar_t delim) {
    std::vector<std::wstring> out;
    size_t start = 0;
    for (size_t i = 0; i <= line.size(); i++) {
        if (i == line.size() || line[i] == delim) {
            out.push_back(line.substr(start, i - start));
            start = i + 1;
        }
    }
    return out;
}

std::string WStringToUtf8(const std::wstring& w) {
    if (w.empty()) return std::string();
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &out[0], len, nullptr, nullptr);
    return out;
}

std::wstring Utf8ToWString(const std::string& s) {
    if (s.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len);
    return out;
}
