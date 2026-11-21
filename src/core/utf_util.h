#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>
#include <string_view>

namespace vakit {

inline std::wstring Utf8ToWide(std::string_view utf8) {
    if (utf8.empty()) return {};
    int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring result(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), result.data(), needed);
    return result;
}

inline std::string WideToUtf8(std::wstring_view wide) {
    if (wide.empty()) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string result(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), result.data(), needed, nullptr, nullptr);
    return result;
}

inline std::wstring NormalizeTurkishLower(std::wstring_view input) {
    std::wstring out;
    out.reserve(input.size());
    for (wchar_t ch : input) {
        switch (ch) {
            case L'İ':
            case L'I':
            case L'ı':
            case L'î':
            case L'Î':
                out.push_back(L'i');
                break;
            case L'Ğ':
            case L'ğ':
                out.push_back(L'g');
                break;
            case L'Ü':
            case L'ü':
            case L'Û':
            case L'û':
                out.push_back(L'u');
                break;
            case L'Ş':
            case L'ş':
                out.push_back(L's');
                break;
            case L'Ö':
            case L'ö':
                out.push_back(L'o');
                break;
            case L'Ç':
            case L'ç':
                out.push_back(L'c');
                break;
            case L'Â':
            case L'â':
                out.push_back(L'a');
                break;
            default:
                if (ch >= L'A' && ch <= L'Z') {
                    out.push_back(static_cast<wchar_t>(ch + (L'a' - L'A')));
                } else {
                    out.push_back(ch);
                }
                break;
        }
    }
    return out;
}

inline std::wstring TrimWide(std::wstring_view sv) {
    size_t start = 0;
    while (start < sv.size() && (sv[start] == L' ' || sv[start] == L'\t' || sv[start] == L'\r' || sv[start] == L'\n')) {
        ++start;
    }
    size_t end = sv.size();
    while (end > start && (sv[end - 1] == L' ' || sv[end - 1] == L'\t' || sv[end - 1] == L'\r' || sv[end - 1] == L'\n')) {
        --end;
    }
    return std::wstring(sv.substr(start, end - start));
}

inline std::string TrimUtf8(std::string_view sv) {
    size_t start = 0;
    while (start < sv.size() && (sv[start] == ' ' || sv[start] == '\t' || sv[start] == '\r' || sv[start] == '\n')) {
        ++start;
    }
    size_t end = sv.size();
    while (end > start && (sv[end - 1] == ' ' || sv[end - 1] == '\t' || sv[end - 1] == '\r' || sv[end - 1] == '\n')) {
        --end;
    }
    return std::string(sv.substr(start, end - start));
}

} // namespace vakit
