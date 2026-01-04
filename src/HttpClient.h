#pragma once
#include <string>

class HttpClient
{
public:
    // Returns true on success and fills responseBody (UTF-8 / raw bytes treated as std::string).
    // Returns false and fills errorMessage on failure.
    bool Get(const std::wstring &url, std::string &responseBody, std::wstring &errorMessage) const;
};
