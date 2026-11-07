#include "MergeManager.h"

#include "Commit.h"
#include "CommitManager.h"
#include "branchmanager.h"

#include <queue>
#include <unordered_set>
#include <utility>
#include <optional>
#include <unordered_map>
#include <unordered_set>


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
bool MergeManager::buildThreeWayMerge(
    const Commit& base,
    const Commit& ours,
    const Commit& theirs,
    std::unordered_map<std::string, std::string>& mergedHashes,
    std::vector<std::string>& conflicts
) const {
    mergedHashes.clear();
    conflicts.clear();

    std::unordered_set<std::string> paths;

    for (const auto& entry : base.getFileHashes()) {
        paths.insert(entry.first);
    }

    for (const auto& entry : ours.getFileHashes()) {
        paths.insert(entry.first);
    }

    for (const auto& entry : theirs.getFileHashes()) {
        paths.insert(entry.first);
    }

    for (const std::string& path : paths) {
        const auto baseIt = base.getFileHashes().find(path);
        const auto oursIt = ours.getFileHashes().find(path);
        const auto theirsIt = theirs.getFileHashes().find(path);

        const bool baseExists = baseIt != base.getFileHashes().end();
        const bool oursExists = oursIt != ours.getFileHashes().end();
        const bool theirsExists = theirsIt != theirs.getFileHashes().end();

        const std::string baseHash =
            baseExists ? baseIt->second : "";

        const std::string oursHash =
            oursExists ? oursIt->second : "";

        const std::string theirsHash =
            theirsExists ? theirsIt->second : "";

        // Both branches have exactly the same state.
        if (oursExists == theirsExists &&
            oursHash == theirsHash) {
            if (oursExists) {
                mergedHashes[path] = oursHash;
            }
            continue;
        }

        // Only theirs changed from the base.
        if (oursExists == baseExists &&
            oursHash == baseHash) {
            if (theirsExists) {
                mergedHashes[path] = theirsHash;
            }
            continue;
        }

        // Only ours changed from the base.
        if (theirsExists == baseExists &&
            theirsHash == baseHash) {
            if (oursExists) {
                mergedHashes[path] = oursHash;
            }
            continue;
        }

        // Both branches changed the path differently.
        conflicts.push_back(path);
    }

    return conflicts.empty();
}

MergeResult MergeManager::mergeBranch(
    const std::string& sourceBranch
) {
    MergeResult result;

    BranchManager branchManager(repositoryRoot);
    CommitManager commitManager(repositoryRoot);

    const std::string currentBranch =
        branchManager.getCurrentBranch();

    const std::string oursId =
        branchManager.getCurrentCommit();

    const std::string theirsId =
        branchManager.getBranchCommit(sourceBranch);

    if (currentBranch.empty() || oursId.empty()) {
        result.message =
            "Could not determine the current branch or commit.";
        return result;
    }

    if (sourceBranch.empty() ||
        !branchManager.branchExists(sourceBranch)) {
        result.message =
            "Source branch does not exist: " + sourceBranch;
        return result;
    }

    if (theirsId.empty()) {
        result.message =
            "Source branch has no commit.";
        return result;
    }

    if (oursId == theirsId) {
        result.success = true;
        result.message =
            "Branches already point to the same commit.";
        return result;
    }

    const std::optional<std::string> ancestorId =
        findCommonAncestor(oursId, theirsId);

    if (!ancestorId.has_value()) {
        result.message =
            "No common ancestor found; merge cannot continue.";
        return result;
    }

    Commit base;
    Commit ours;
    Commit theirs;

    if (!commitManager.getCommit(*ancestorId, base) ||
        !commitManager.getCommit(oursId, ours) ||
        !commitManager.getCommit(theirsId, theirs)) {
        result.message =
            "Could not load merge commit metadata.";
        return result;
    }

    std::unordered_map<std::string, std::string> mergedHashes;

    if (!buildThreeWayMerge(
            base,
            ours,
            theirs,
            mergedHashes,
            result.conflicts)) {
        result.success = false;
        result.hasConflicts = true;
        result.message =
            "Merge stopped because file conflicts were detected.";
        return result;
    }

    if (mergedHashes.empty()) {
        result.message =
            "Merge produced no files.";
        return result;
    }

    std::string mergeCommitId;

    const std::string authorId =
        ours.getAuthorId().empty()
            ? "merge"
            : ours.getAuthorId();

    const bool created =
        commitManager.createMergeCommit(
            "Merge branch '" + sourceBranch + "'",
            authorId,
            oursId,
            theirsId,
            mergedHashes,
            mergeCommitId
        );

    if (!created) {
        result.message =
            "Could not create the merge commit.";
        return result;
    }

    if (!branchManager.getCurrentBranch().empty() &&
        !commitManager.updateBranchPointer(
            currentBranch,
            mergeCommitId
        )) {
        result.message =
            "Merge commit was created, but branch pointer update failed.";
        return result;
    }

    result.success = true;
    result.hasConflicts = false;
    result.message =
        "Merge completed successfully. Created commit " +
        mergeCommitId + ".";

    return result;
}
