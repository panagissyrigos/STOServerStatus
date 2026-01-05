#pragma once
#include <string>

namespace Crypto
{
    // AES-256-CBC encryption, Base64 output
    std::wstring EncryptString(const wchar_t* passphrase,
        const wchar_t* plaintext);

    // AES-256-CBC decryption from Base64
    std::wstring DecryptString(const wchar_t* passphrase,
        const wchar_t* base64Cipher);
}
