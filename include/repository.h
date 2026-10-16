
#ifndef REPOSITORY_H
#define REPOSITORY_H

#include <filesystem>

class Repository {
private:
    std::filesystem::path rootPath;

public:
    explicit Repository(const std::filesystem::path& path);

    bool init() const;
    bool isInitialized() const;

    std::filesystem::path getRootPath() const;
    std::filesystem::path getMetadataPath() const;
};

#endif
