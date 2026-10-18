#ifndef COMMITMANAGER_H
#define COMMITMANAGER_H

#include "Commit.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

class CommitManager {
private:
    std::filesystem::path repositoryRoot;
    std::filesystem::path metadataPath;
    std::filesystem::path objectsPath;
    
    bool readCommitFile(const std::string& commitId, Commit& out) const;

    std::vector<std::string> findOverwriteConflicts(
        const std::vector<std::string>& paths,
        const std::unordered_map<std::string, std::string>& hashes) const;

public:
    explicit CommitManager(
        const std::filesystem::path& rootPath
    );

    bool createCommit(const std::string& message);

    std::vector<Commit> getCommitHistory() const;

    std::string getLatestCommitId() const;
    bool restoreCommit(const std::string& requestedId, bool force = false);

    std::string resolveCommitId(const std::string& prefix) const;

    bool getCommit(const std::string& requestedId, Commit& out) const;
.
    std::filesystem::path getSnapshotPath(const std::string& commitId) const;

    bool isSnapshotPruned(const std::string& commitId) const;

    std::vector<std::string> getBranchTips() const;
};

#endif