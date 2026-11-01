#include "MergeManager.h"

#include <utility>

MergeManager::MergeManager(
    const std::filesystem::path& rootPath
)
    : repositoryRoot(rootPath) {
}

MergeResult MergeManager::mergeBranch(
    const std::string& sourceBranch
) {
    MergeResult result;

    result.success = false;
    result.hasConflicts = false;
    result.message =
        "Merge implementation is not complete. "
        "Commit format agreement is required first.";

    return result;
}
