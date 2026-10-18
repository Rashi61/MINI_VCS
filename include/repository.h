#ifndef REPOSITORY_H
#define REPOSITORY_H

#include <filesystem>
#include <optional>

class Repository {
private:
    std::filesystem::path rootPath;

public:
    explicit Repository(const std::filesystem::path& path);

    static std::optional<std::filesystem::path>
    discoverRoot(const std::filesystem::path& start);

    bool init() const;
    bool isInitialized() const;

    std::filesystem::path getRootPath() const;
    std::filesystem::path getMetadataPath() const;
};

#endif
