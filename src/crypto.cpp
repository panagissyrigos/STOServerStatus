#include "Crypto.h"

#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <vector>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")

namespace
{
    // Fixed IV for deterministic encryption (16 bytes for AES)
    // If you ever want better security, randomize this and store/transmit it.
    const BYTE FIXED_IV[16] = {
        0x10, 0x23, 0x45, 0x67,
        0x89, 0xAB, 0xCD, 0xEF,
        0x01, 0x12, 0x23, 0x34,
        0x45, 0x56, 0x67, 0x78
    };

    bool DeriveKeyFromPassphrase(const std::wstring& passphrase,
        std::vector<BYTE>& keyOut)
    {
        keyOut.clear();
        keyOut.resize(32); // AES-256 key

        BCRYPT_ALG_HANDLE hHashAlg = nullptr;
        BCRYPT_HASH_HANDLE hHash = nullptr;

        NTSTATUS status = BCryptOpenAlgorithmProvider(
            &hHashAlg,
            BCRYPT_SHA256_ALGORITHM,
            nullptr,
            0
        );
        if (status < 0)
            return false;

        DWORD hashObjectSize = 0;
        DWORD result = 0;
        status = BCryptGetProperty(
            hHashAlg,
            BCRYPT_OBJECT_LENGTH,
            (PUCHAR)&hashObjectSize,
            sizeof(hashObjectSize),
            &result,
            0
        );
        if (status < 0)
        {
            BCryptCloseAlgorithmProvider(hHashAlg, 0);
            return false;
        }

        std::vector<BYTE> hashObject(hashObjectSize);

        DWORD hashLength = 0;
        status = BCryptGetProperty(
            hHashAlg,
            BCRYPT_HASH_LENGTH,
            (PUCHAR)&hashLength,
            sizeof(hashLength),
            &result,
            0
        );
        if (status < 0 || hashLength != 32)
        {
            BCryptCloseAlgorithmProvider(hHashAlg, 0);
            return false;
        }

        status = BCryptCreateHash(
            hHashAlg,
            &hHash,
            hashObject.data(),
            hashObjectSize,
            nullptr,
            0,
            0
        );
        if (status < 0)
        {
            BCryptCloseAlgorithmProvider(hHashAlg, 0);
            return false;
        }

        const BYTE* passBytes =
            reinterpret_cast<const BYTE*>(passphrase.data());
        const DWORD passLenBytes =
            static_cast<DWORD>(passphrase.size() * sizeof(wchar_t));

        status = BCryptHashData(
            hHash,
            (PUCHAR)passBytes,
            passLenBytes,
            0
        );
        if (status < 0)
        {
            BCryptDestroyHash(hHash);
            BCryptCloseAlgorithmProvider(hHashAlg, 0);
            return false;
        }

        status = BCryptFinishHash(
            hHash,
            keyOut.data(),
            (ULONG)keyOut.size(),
            0
        );

        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hHashAlg, 0);

        return status >= 0;
    }
}

std::wstring Crypto::EncryptString(const wchar_t* passphrase,
    const wchar_t* plaintext)
{
    if (!passphrase || !plaintext)
        return {};

    std::wstring pass(passphrase);
    std::wstring plain(plaintext);

    // Derive key from passphrase
    std::vector<BYTE> key;
    if (!DeriveKeyFromPassphrase(pass, key))
        return {};

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;

    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &hAlg,
        BCRYPT_AES_ALGORITHM,
        nullptr,
        0
    );
    if (status < 0)
        return {};

    status = BCryptSetProperty(
        hAlg,
        BCRYPT_CHAINING_MODE,
        (PUCHAR)BCRYPT_CHAIN_MODE_CBC,
        (ULONG)(wcslen(BCRYPT_CHAIN_MODE_CBC) + 1) * sizeof(wchar_t),
        0
    );
    if (status < 0)
    {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    DWORD keyObjectSize = 0;
    DWORD result = 0;
    status = BCryptGetProperty(
        hAlg,
        BCRYPT_OBJECT_LENGTH,
        (PUCHAR)&keyObjectSize,
        sizeof(keyObjectSize),
        &result,
        0
    );
    if (status < 0)
    {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    std::vector<BYTE> keyObject(keyObjectSize);

    status = BCryptGenerateSymmetricKey(
        hAlg,
        &hKey,
        keyObject.data(),
        (ULONG)keyObject.size(),
        key.data(),
        (ULONG)key.size(),
        0
    );
    if (status < 0)
    {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    // Prepare IV
    BYTE iv[16];
    memcpy(iv, FIXED_IV, sizeof(iv));

    const BYTE* plainBytes =
        reinterpret_cast<const BYTE*>(plain.data());
    ULONG plainSize =
        (ULONG)(plain.size() * sizeof(wchar_t));

    ULONG cipherSize = 0;
    status = BCryptEncrypt(
        hKey,
        (PUCHAR)plainBytes,
        plainSize,
        nullptr,
        iv,
        sizeof(iv),
        nullptr,
        0,
        &cipherSize,
        BCRYPT_BLOCK_PADDING
    );
    if (status < 0)
    {
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    std::vector<BYTE> cipher(cipherSize);

    memcpy(iv, FIXED_IV, sizeof(iv));
    status = BCryptEncrypt(
        hKey,
        (PUCHAR)plainBytes,
        plainSize,
        nullptr,
        iv,
        sizeof(iv),
        cipher.data(),
        cipherSize,
        &cipherSize,
        BCRYPT_BLOCK_PADDING
    );
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (status < 0)
        return {};

    // Base64 encode
    DWORD b64Len = 0;
    if (!CryptBinaryToStringW(
        cipher.data(), cipherSize,
        CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
        nullptr, &b64Len))
    {
        return {};
    }

    std::wstring base64(b64Len, L'\0');
    if (!CryptBinaryToStringW(
        cipher.data(), cipherSize,
        CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
        base64.data(), &b64Len))
    {
        return {};
    }

    if (!base64.empty() && base64.back() == L'\0')
        base64.pop_back();

    return base64;
}

std::wstring Crypto::DecryptString(const wchar_t* passphrase,
    const wchar_t* base64Cipher)
{
    if (!passphrase || !base64Cipher)
        return {};

    std::wstring pass(passphrase);
    std::wstring b64(base64Cipher);

    // Derive key
    std::vector<BYTE> key;
    if (!DeriveKeyFromPassphrase(pass, key))
        return {};

    // Base64 decode
    DWORD binSize = 0;
    if (!CryptStringToBinaryW(
        b64.c_str(), 0,
        CRYPT_STRING_BASE64,
        nullptr, &binSize,
        nullptr, nullptr))
    {
        return {};
    }

    std::vector<BYTE> cipher(binSize);
    if (!CryptStringToBinaryW(
        b64.c_str(), 0,
        CRYPT_STRING_BASE64,
        cipher.data(), &binSize,
        nullptr, nullptr))
    {
        return {};
    }

    cipher.resize(binSize);

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;

    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &hAlg,
        BCRYPT_AES_ALGORITHM,
        nullptr,
        0
    );
    if (status < 0)
        return {};

    status = BCryptSetProperty(
        hAlg,
        BCRYPT_CHAINING_MODE,
        (PUCHAR)BCRYPT_CHAIN_MODE_CBC,
        (ULONG)(wcslen(BCRYPT_CHAIN_MODE_CBC) + 1) * sizeof(wchar_t),
        0
    );
    if (status < 0)
    {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    DWORD keyObjectSize = 0;
    DWORD result = 0;
    status = BCryptGetProperty(
        hAlg,
        BCRYPT_OBJECT_LENGTH,
        (PUCHAR)&keyObjectSize,
        sizeof(keyObjectSize),
        &result,
        0
    );
    if (status < 0)
    {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    std::vector<BYTE> keyObject(keyObjectSize);

    status = BCryptGenerateSymmetricKey(
        hAlg,
        &hKey,
        keyObject.data(),
        (ULONG)keyObject.size(),
        key.data(),
        (ULONG)key.size(),
        0
    );
    if (status < 0)
    {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    BYTE iv[16];
    memcpy(iv, FIXED_IV, sizeof(iv));

    ULONG plainSize = 0;
    status = BCryptDecrypt(
        hKey,
        cipher.data(),
        (ULONG)cipher.size(),
        nullptr,
        iv,
        sizeof(iv),
        nullptr,
        0,
        &plainSize,
        BCRYPT_BLOCK_PADDING
    );
    if (status < 0)
    {
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    std::vector<BYTE> plain(plainSize);

    memcpy(iv, FIXED_IV, sizeof(iv));
    status = BCryptDecrypt(
        hKey,
        cipher.data(),
        (ULONG)cipher.size(),
        nullptr,
        iv,
        sizeof(iv),
        plain.data(),
        plainSize,
        &plainSize,
        BCRYPT_BLOCK_PADDING
    );
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (status < 0)
        return {};

    if (plainSize % sizeof(wchar_t) != 0)
        return {};

    return std::wstring(
        reinterpret_cast<wchar_t*>(plain.data()),
        plainSize / sizeof(wchar_t));
}
