#pragma once
#include <string>

namespace Crypto {

    std::wstring DecryptString(const wchar_t* passphrase,
        const wchar_t* base64Cipher);
}
