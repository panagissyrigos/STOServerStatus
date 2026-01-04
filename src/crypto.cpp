#include <windows.h>
#include <wincrypt.h>
#include <string>
#include <vector>

#pragma comment(lib, "crypt32.lib")


namespace Crypto {


    std::wstring DecryptString(const wchar_t* passphrase,
        const wchar_t* base64Cipher)
    {
        // Base64 decode (Unicode-safe)
        DWORD binSize = 0;
        CryptStringToBinaryW(
            base64Cipher, 0,
            CRYPT_STRING_BASE64,
            nullptr, &binSize,
            nullptr, nullptr
        );

        std::vector<BYTE> encrypted(binSize);
        CryptStringToBinaryW(
            base64Cipher, 0,
            CRYPT_STRING_BASE64,
            encrypted.data(), &binSize,
            nullptr, nullptr
        );

        DATA_BLOB inBlob{
            binSize,
            encrypted.data()
        };

        DATA_BLOB entropy{
            static_cast<DWORD>(wcslen(passphrase) * sizeof(wchar_t)),
            reinterpret_cast<BYTE*>(const_cast<wchar_t*>(passphrase))
        };

        DATA_BLOB outBlob{};

        if (!CryptUnprotectData(
            &inBlob,
            nullptr,
            &entropy,
            nullptr,
            nullptr,
            0,
            &outBlob))
        {
            return {};
        }

        std::wstring result(
            reinterpret_cast<wchar_t*>(outBlob.pbData),
            outBlob.cbData / sizeof(wchar_t)
        );

        LocalFree(outBlob.pbData);
        return result;
    }


}
