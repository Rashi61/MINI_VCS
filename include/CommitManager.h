
#ifndef COMMITMANAGER_H
#define COMMITMANAGER_H

#include "Commit.h"

#include <filesystem>
#include <string>
#include <vector>

class CommitManager {
private:
    std::filesystem::path repositoryRoot;
    std::filesystem::path metadataPath;
    std::filesystem::path objectsPath;

public:
    explicit CommitManager(
        const std::filesystem::path& rootPath
    );

    bool createCommit(const std::string& message);

    std::vector<Commit> getCommitHistory() const;

    std::string getLatestCommitId() const;
};

#endif
