
#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <filesystem>
#include <string>
#include <vector>

class FileManager {
private:
    std::filesystem::path repositoryRoot;
    std::filesystem::path metadataPath;
    std::filesystem::path trackedFilesPath;

public:
    explicit FileManager(const std::filesystem::path& rootPath);

    bool addFile(const std::string& relativePath);
    bool removeFile(const std::string& relativePath);

    std::vector<std::string> getTrackedFiles() const;

    bool saveTrackedFiles(
        const std::vector<std::string>& files
    ) const;

    bool loadTrackedFiles(
        std::vector<std::string>& files
    ) const;
};

#endif
