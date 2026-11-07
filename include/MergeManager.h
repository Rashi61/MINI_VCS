#ifndef MERGE_MANAGER_H
#define MERGE_MANAGER_H

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

struct MergeResult {
    bool success = false;
    bool hasConflicts = false;
    std::string message;
    std::vector<std::string> conflicts;
};

class MergeManager {
private:
    std::filesystem::path repositoryRoot;

    bool collectAncestors(
        const std::string& commitId,
        std::unordered_set<std::string>& ancestors
    ) const;

    std::optional<std::string> findCommonAncestor(
        const std::string& ours,
        const std::string& theirs
    ) const;

public:
    explicit MergeManager(
        const std::filesystem::path& rootPath
    );

    MergeResult mergeBranch(
        const std::string& sourceBranch
    );
};

#endif
