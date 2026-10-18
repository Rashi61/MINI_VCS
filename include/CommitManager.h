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

    // Reads one commit's metadata by full ID. Returns false if unreadable.
    bool readCommitFile(const std::string& commitId, Commit& out) const;

    // Files in 'paths' whose current content exists in no commit and
    // differs from the snapshot: overwriting them would lose work.
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

    // Resolves a full ID or a unique prefix (min. 4 chars) to a full ID.
    // Prints an error and returns "" if missing or ambiguous.
    std::string resolveCommitId(const std::string& prefix) const;

    // Loads any commit (not only the current branch's chain) by ID or
    // unique prefix. Prints an error and returns false on failure.
    bool getCommit(const std::string& requestedId, Commit& out) const;

    // objects/<id>/files : the stored copy of the commit's files.
    std::filesystem::path getSnapshotPath(const std::string& commitId) const;

    // True if the retention policy removed this commit's stored files.
    bool isSnapshotPruned(const std::string& commitId) const;

    // Commit IDs that every branch pointer currently points to.
    std::vector<std::string> getBranchTips() const;
};

#endif