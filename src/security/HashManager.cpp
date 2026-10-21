#include "HashManager.h"

#include <openssl/evp.h>

#include <string>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>

std::string HashManager::calculateHash(
    const std::filesystem::path& filePath
) {
    std::ifstream input(filePath, std::ios::binary);

    if (!input) {
        return "";
    }

    EVP_MD_CTX* context = EVP_MD_CTX_new();

    if (context == nullptr) {
        return "";
    }
    if (EVP_DigestInit_ex(
            context, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(context);
        return "";
    }

    std::array<char, 8192> buffer{};

    while (input) {
        input.read(
            buffer.data(),
            static_cast<std::streamsize>(buffer.size())
        );
        const std::streamsize bytesRead = input.gcount();
        if (bytesRead > 0) {
            if (EVP_DigestUpdate(
                    context,
                    buffer.data(),
                    static_cast<std::size_t>(bytesRead)) != 1) {
                EVP_MD_CTX_free(context);
                return "";
            }
        }
    }
    if (input.bad()) {
        EVP_MD_CTX_free(context);
        return "";
    }

    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digestLength = 0;

    if (EVP_DigestFinal_ex(
            context, digest.data(), &digestLength) != 1) {
        EVP_MD_CTX_free(context);
        return "";
    }

    EVP_MD_CTX_free(context);

    std::ostringstream result;
    result << std::hex << std::setfill('0');

    for (unsigned int i = 0; i < digestLength; ++i) {
        result << std::setw(2)
               << static_cast<unsigned int>(digest[i]);
    }
    return result.str();
}

bool HashManager::hasChanged(
    const std::filesystem::path& filePath,
    const std::string& previousHash
) {
    const std::string currentHash = calculateHash(filePath);
    if (currentHash.empty()) {
        return true;
    }

    return currentHash != previousHash;
}

std::string HashManager::calculateHashFromString(
    const std::string& data
) {
    EVP_MD_CTX* context = EVP_MD_CTX_new();

    if (context == nullptr) {
        return "";
    }

    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digestLength = 0;

    if (EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(context, data.data(), data.size()) != 1 ||
        EVP_DigestFinal_ex(
            context, digest.data(), &digestLength) != 1) {
        EVP_MD_CTX_free(context);
        return "";
    }

    EVP_MD_CTX_free(context);

    std::ostringstream result;
    result << std::hex << std::setfill('0');

    for (unsigned int i = 0; i < digestLength; ++i) {
        result << std::setw(2)
               << static_cast<unsigned int>(digest[i]);
    }

    return result.str();
}
bool HashManager::tryCalculateHash(
    const std::filesystem::path& filePath,
    std::string& hashOut
) {
    hashOut.clear();

    std::string hash = calculateHash(filePath);

    if (hash.empty()) {
        return false;
    }

    hashOut = std::move(hash);
    return true;
}