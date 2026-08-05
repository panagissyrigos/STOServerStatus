#include "HttpClient.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <stdexcept>

#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace
{
    struct WinHttpHandle
    {
        HINTERNET h = nullptr;
        WinHttpHandle() = default;
        explicit WinHttpHandle(HINTERNET handle) : h(handle) {}
        ~WinHttpHandle()
        {
            if (h)
                WinHttpCloseHandle(h);
        }
        WinHttpHandle(const WinHttpHandle &) = delete;
        WinHttpHandle &operator=(const WinHttpHandle &) = delete;
        WinHttpHandle(WinHttpHandle &&o) noexcept : h(o.h) { o.h = nullptr; }
        WinHttpHandle &operator=(WinHttpHandle &&o) noexcept
        {
            if (this != &o)
            {
                if (h)
                    WinHttpCloseHandle(h);
                h = o.h;
                o.h = nullptr;
            }
            return *this;
        }
        operator HINTERNET() const { return h; }
        explicit operator bool() const { return h != nullptr; }
    };

    std::wstring WinHttpErrorMessage(DWORD err)
    {
        wchar_t *buf = nullptr;
        DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
        DWORD len = FormatMessageW(flags, nullptr, err, 0, reinterpret_cast<wchar_t *>(&buf), 0, nullptr);
        std::wstring msg = (len && buf) ? std::wstring(buf, buf + len) : L"(unknown WinHTTP error)";
        if (buf)
            LocalFree(buf);
        return msg;
    }
}

bool HttpClient::Get(const std::wstring &url, std::string &responseBody, std::wstring &errorMessage) const
{
    try {
        responseBody.clear();
        errorMessage.clear();

        URL_COMPONENTS uc{};
        uc.dwStructSize = sizeof(uc);

        wchar_t host[256]{};
        wchar_t path[2048]{};

        uc.lpszHostName = host;
        uc.dwHostNameLength = static_cast<DWORD>(std::size(host));
        uc.lpszUrlPath = path;
        uc.dwUrlPathLength = static_cast<DWORD>(std::size(path));

        if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc))
        {
            errorMessage = L"WinHttpCrackUrl failed: " + WinHttpErrorMessage(GetLastError());
            return false;
        }

        const bool isHttps = (uc.nScheme == INTERNET_SCHEME_HTTPS);
        const INTERNET_PORT port = uc.nPort;

        WinHttpHandle session(WinHttpOpen(L"stostatus-tray/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0));
        if (!session)
        {
            errorMessage = L"WinHttpOpen failed: " + WinHttpErrorMessage(GetLastError());
            return false;
        }

        // Keep timeouts sane for a tray utility.
        WinHttpSetTimeouts(session, 5000, 5000, 5000, 5000);

        WinHttpHandle connect(WinHttpConnect(session, std::wstring(host, uc.dwHostNameLength).c_str(), port, 0));
        if (!connect)
        {
            errorMessage = L"WinHttpConnect failed: " + WinHttpErrorMessage(GetLastError());
            return false;
        }

        const std::wstring urlPath = std::wstring(path, uc.dwUrlPathLength);
        WinHttpHandle request(WinHttpOpenRequest(connect,
            L"GET",
            urlPath.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            isHttps ? WINHTTP_FLAG_SECURE : 0));
        if (!request)
        {
            errorMessage = L"WinHttpOpenRequest failed: " + WinHttpErrorMessage(GetLastError());
            return false;
        }

        // Helps some endpoints; harmless otherwise.
        WinHttpAddRequestHeaders(request, L"Accept: */*\r\n", -1L, WINHTTP_ADDREQ_FLAG_ADD);

        if (!WinHttpSendRequest(request,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0))
        {
            errorMessage = L"WinHttpSendRequest failed: " + WinHttpErrorMessage(GetLastError());
            return false;
        }

        if (!WinHttpReceiveResponse(request, nullptr))
        {
            errorMessage = L"WinHttpReceiveResponse failed: " + WinHttpErrorMessage(GetLastError());
            return false;
        }

        std::vector<char> buffer;
        for (;;)
        {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(request, &available))
            {
                errorMessage = L"WinHttpQueryDataAvailable failed: " + WinHttpErrorMessage(GetLastError());
                return false;
            }
            if (available == 0)
                break;

            const size_t oldSize = buffer.size();
            buffer.resize(oldSize + available);

            DWORD read = 0;
            if (!WinHttpReadData(request, buffer.data() + oldSize, available, &read))
            {
                errorMessage = L"WinHttpReadData failed: " + WinHttpErrorMessage(GetLastError());
                return false;
            }

            // If server lied about 'available', shrink to actual read bytes.
            buffer.resize(oldSize + read);
        }

        responseBody.assign(buffer.begin(), buffer.end());
        return true;
    }
    //catch (...) {
    catch (const std::exception& ex) {
        MessageBoxA(nullptr, ex.what(), "STO Status Tray - Error", MB_OK | MB_ICONERROR);
        return false;
    }
}
