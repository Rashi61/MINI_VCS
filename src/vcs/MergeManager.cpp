#include "MergeManager.h"

#include "Commit.h"
#include "CommitManager.h"

#include <queue>
#include <unordered_set>
#include <utility>

MergeManager::MergeManager(
    const std::filesystem::path& rootPath
)
    : repositoryRoot(rootPath) {
}

bool MergeManager::collectAncestors(
    const std::string& commitId,
    std::unordered_set<std::string>& ancestors
) const {
    if (commitId.empty()) {
        return true;
    }

    if (!ancestors.insert(commitId).second) {
        return true;
    }

    CommitManager commitManager(repositoryRoot);
    Commit commit;

    if (!commitManager.getCommit(commitId, commit)) {
        return false;
    }

    if (!collectAncestors(commit.getFirstParentId(), ancestors)) {
        return false;
    }

    if (!collectAncestors(commit.getSecondParentId(), ancestors)) {
        return false;
    }

    return true;
}

std::optional<std::string> MergeManager::findCommonAncestor(
    const std::string& ours,
    const std::string& theirs
) const {
    std::unordered_set<std::string> oursAncestors;

    if (!collectAncestors(ours, oursAncestors)) {
        return std::nullopt;
    }

    CommitManager commitManager(repositoryRoot);

    std::queue<std::string> pending;
    std::unordered_set<std::string> visited;

    pending.push(theirs);

    while (!pending.empty()) {
        const std::string current = pending.front();
        pending.pop();

        if (current.empty()) {
            continue;
        }

        if (!visited.insert(current).second) {
            continue;
        }

        if (oursAncestors.find(current) != oursAncestors.end()) {
            return current;
        }

        Commit commit;

        if (!commitManager.getCommit(current, commit)) {
            return std::nullopt;
        }

        if (!commit.getFirstParentId().empty()) {
            pending.push(commit.getFirstParentId());
        }

        if (!commit.getSecondParentId().empty()) {
            pending.push(commit.getSecondParentId());
        }
    }

    return std::nullopt;
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
