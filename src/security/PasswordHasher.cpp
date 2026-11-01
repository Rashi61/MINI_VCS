#include "PasswordHasher.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <vector>

namespace {

constexpr int SALT_SIZE = 16;
constexpr int HASH_SIZE = 32;
constexpr int ITERATIONS = 310000;

std::string bytesToHex(
    const unsigned char* bytes,
    std::size_t size
) {
    const char* digits = "0123456789abcdef";
    std::string result;
    result.reserve(size * 2);

    for (std::size_t i = 0; i < size; ++i) {
        result += digits[bytes[i] >> 4];
        result += digits[bytes[i] & 0x0F];
    }

    return result;
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    return -1;
}

bool hexToBytes(
    const std::string& text,
    std::vector<unsigned char>& result
) {
    if (text.size() % 2 != 0) {
        return false;
    }

    result.clear();
    result.reserve(text.size() / 2);

    for (std::size_t i = 0; i < text.size(); i += 2) {
        const int high = hexValue(text[i]);
        const int low = hexValue(text[i + 1]);

        if (high < 0 || low < 0) {
            return false;
        }

        result.push_back(
            static_cast<unsigned char>((high << 4) | low)
        );
    }

    return true;
}

bool deriveHash(
    const std::string& password,
    const std::vector<unsigned char>& salt,
    std::vector<unsigned char>& hash
) {
    hash.assign(HASH_SIZE, 0);

    const int result = PKCS5_PBKDF2_HMAC(
        password.data(),
        static_cast<int>(password.size()),
        salt.data(),
        static_cast<int>(salt.size()),
        ITERATIONS,
        EVP_sha256(),
        static_cast<int>(hash.size()),
        hash.data()
    );

    return result == 1;
}

} // namespace

bool PasswordHasher::createHash(
    const std::string& password,
    PasswordHash& result
) {
    if (password.empty()) {
        return false;
    }

    std::vector<unsigned char> salt(SALT_SIZE);

    if (RAND_bytes(
            salt.data(),
            static_cast<int>(salt.size())
        ) != 1) {
        return false;
    }

    std::vector<unsigned char> hash;

    if (!deriveHash(password, salt, hash)) {
        return false;
    }

    result.salt = bytesToHex(
        salt.data(),
        salt.size()
    );

    result.hash = bytesToHex(
        hash.data(),
        hash.size()
    );

    return true;
}

bool PasswordHasher::verify(
    const std::string& password,
    const std::string& storedHash,
    const std::string& storedSalt
) {
    if (password.empty()) {
        return false;
    }

    std::vector<unsigned char> expectedHash;
    std::vector<unsigned char> salt;

    if (!hexToBytes(storedHash, expectedHash) ||
        !hexToBytes(storedSalt, salt)) {
        return false;
    }

    if (expectedHash.size() != HASH_SIZE ||
        salt.size() != SALT_SIZE) {
        return false;
    }

    std::vector<unsigned char> calculatedHash;

    if (!deriveHash(password, salt, calculatedHash)) {
        return false;
    }

    return CRYPTO_memcmp(
        expectedHash.data(),
        calculatedHash.data(),
        expectedHash.size()
    ) == 0;
}