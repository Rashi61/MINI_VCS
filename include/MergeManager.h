#ifndef MERGE_MANAGER_H
#define MERGE_MANAGER_H

#include <filesystem>
#include <string>
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

public:
    explicit MergeManager(
        const std::filesystem::path& rootPath
    );

    MergeResult mergeBranch(
        const std::string& sourceBranch
    );
};

#endif
