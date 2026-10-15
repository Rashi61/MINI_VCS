
#ifndef HASHMANAGER_H
#define HASHMANAGER_H

#include <filesystem>
#include <string>

class HashManager {
public:
    static std::string calculateHash(
        const std::filesystem::path& filePath
    );

    static bool hasChanged(
        const std::filesystem::path& filePath,
        const std::string& previousHash
    );

    static bool tryCalculateHash(
    const std::filesystem::path& filePath,
    std::string& hashOut
    );
};

#endif
