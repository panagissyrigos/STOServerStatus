#include "StatusChecker.h"
#include "HttpClient.h"
#include <windows.h>

#include <algorithm>

namespace
{
    std::string ToLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    bool Contains(const std::string &haystackLower, const char *needleLower)
    {
        return haystackLower.find(needleLower) != std::string::npos;
    }
}

StatusChecker::StatusChecker(std::wstring statusUrl)
    : statusUrl_(std::move(statusUrl)) {}

StatusResult StatusChecker::CheckOnce() const
{
    HttpClient http;
    std::string body;
    std::wstring err;

    if (!http.Get(statusUrl_, body, err))
    {
        StatusResult r;
        r.state = ServerState::Unknown;
        r.detail = L"Network error: " + err;
        return r;
    }

    return InterpretBody(body);
}

StatusResult StatusChecker::InterpretBody(const std::string &body)
{
    StatusResult r;

    const std::string lower = ToLower(body);
    if (Contains(lower, "\"server_status\":\"down\""))
    {
        r.state = ServerState::Maintenance;
        r.detail = L"Maintenance / Offline (matched response text)";
        return r;
    }

    if (Contains(lower, "\"server_status\":\"up\""))
    {
        r.state = ServerState::Online;
        r.detail = L"Online (matched response text)";
        return r;
    }

    r.state = ServerState::Unknown;
    r.detail = L"Unknown response (no match)";
    return r;
}
