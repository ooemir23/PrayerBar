#include "win_http.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>

namespace vakit {

namespace {

std::optional<std::string> WinHttpExecuteUtf8(
    const wchar_t* verb,
    const std::wstring& host,
    const std::wstring& pathAndQuery,
    const wchar_t* extraHeaders,
    const std::string* bodyData,
    bool useHttps,
    int timeoutMs) {

    HINTERNET hSession = WinHttpOpen(
        L"PrayerBar/2.1",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession) return std::nullopt;

    WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    HINTERNET hConnect = WinHttpConnect(
        hSession,
        host.c_str(),
        useHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT,
        0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return std::nullopt;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        verb,
        pathAndQuery.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        useHttps ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return std::nullopt;
    }

    LPVOID optData = bodyData && !bodyData->empty()
        ? const_cast<char*>(bodyData->data())
        : WINHTTP_NO_REQUEST_DATA;
    DWORD optLen = bodyData ? static_cast<DWORD>(bodyData->size()) : 0;
    DWORD hdrLen = extraHeaders ? static_cast<DWORD>(wcslen(extraHeaders)) : 0;

    std::optional<std::string> result;
    if (WinHttpSendRequest(hRequest,
                           extraHeaders ? extraHeaders : WINHTTP_NO_ADDITIONAL_HEADERS,
                           hdrLen,
                           optData,
                           optLen,
                           optLen,
                           0) &&
        WinHttpReceiveResponse(hRequest, nullptr)) {

        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

        if (statusCode >= 200 && statusCode < 500) {
            std::string body;
            body.reserve(16384);
            DWORD avail = 0;
            while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
                if (body.size() + avail > 2 * 1024 * 1024) break;
                size_t prev = body.size();
                body.resize(prev + avail);
                DWORD readBytes = 0;
                if (!WinHttpReadData(hRequest, body.data() + prev, avail, &readBytes) || readBytes == 0) {
                    body.resize(prev);
                    break;
                }
                body.resize(prev + readBytes);
            }
            if (!body.empty() && (statusCode < 300 || body[0] == '{')) {
                result = std::move(body);
            }
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

} // namespace

std::optional<std::string> WinHttpGetUtf8(const std::wstring& host, const std::wstring& pathAndQuery, bool useHttps, int timeoutMs) {
    return WinHttpExecuteUtf8(L"GET", host, pathAndQuery, nullptr, nullptr, useHttps, timeoutMs);
}

std::optional<std::string> WinHttpPostFormUtf8(const std::wstring& host, const std::wstring& path, const std::string& formBody, int timeoutMs) {
    return WinHttpExecuteUtf8(
        L"POST",
        host,
        path,
        L"Content-Type: application/x-www-form-urlencoded\r\nAccept: application/json\r\n",
        &formBody,
        true,
        timeoutMs);
}

} // namespace vakit
