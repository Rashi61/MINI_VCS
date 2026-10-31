#include "IntegrityManager.h"

#include "CommitManager.h"
#include "HashManager.h"
#include "repository.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <system_error>
#include <unordered_set>

namespace fs = std::filesystem;

IntegrityManager::IntegrityManager(const fs::path& rootPath)
    : repositoryRoot(fs::absolute(rootPath).lexically_normal()),
      metadataPath(repositoryRoot / ".minivcs"),
      objectsPath(metadataPath / "objects") {
}

void IntegrityManager::addError(
    IntegrityReport& report,
    const std::string& category,
    const std::string& path,
    const std::string& message
) const {
    report.valid = false;
    report.issues.push_back(
        IntegrityIssue{false, category, path, message}
    );
}

void IntegrityManager::addWarning(
    IntegrityReport& report,
    const std::string& category,
    const std::string& path,
    const std::string& message
) const {
    report.issues.push_back(
        IntegrityIssue{true, category, path, message}
    );
}

bool IntegrityManager::isSafeRelativePath(
    const fs::path& relative
) const {
    if (relative.empty() || relative.is_absolute() ||
        relative.has_root_name() || relative.has_root_directory()) {
        return false;
    }

    const fs::path normalized = relative.lexically_normal();

    if (normalized.empty() || normalized == ".") {
        return false;
    }

    for (const auto& component : normalized) {
        const std::string value = component.string();
        if (value.empty() || value == "." || value == "..") {
            return false;
        }
        if (value == ".minivcs") {
            return false;
        }
    }

    return true;
}

bool IntegrityManager::verifyCommitSnapshot(
    const std::string& commitId,
    IntegrityReport& report
) const {
    CommitManager commitManager(repositoryRoot);
    Commit commit;

    if (!commitManager.getCommit(commitId, commit)) {
        addError(
            report,
            "commit",
            commitId,
            "Commit metadata could not be read."
        );
        return false;
    }

    ++report.checkedCommits;

    const fs::path commitDirectory = objectsPath / commitId;
    const fs::path commitFile = commitDirectory / "commit.txt";
    const fs::path snapshotDirectory = commitManager.getSnapshotPath(commitId);

    std::error_code ec;

    if (!fs::is_regular_file(commitFile, ec) || ec) {
        addError(
            report,
            "commit",
            commitId,
            "commit.txt is missing or is not a regular file."
        );
        return false;
    }

    const std::string parentId = commit.getParentId();
    if (!parentId.empty()) {
        Commit parent;
        if (!commitManager.getCommit(parentId, parent)) {
            addError(
                report,
                "parent",
                commitId,
                "Parent commit does not exist: " + parentId
            );
        }
    }

    if (commitManager.isSnapshotPruned(commitId)) {
        addWarning(
            report,
            "snapshot",
            commitId,
            "Snapshot is pruned; commit history is still available."
        );
        return true;
    }

    ec.clear();
    if (!fs::is_directory(snapshotDirectory, ec) || ec) {
        addError(
            report,
            "snapshot",
            commitId,
            "Snapshot directory is missing."
        );
        return false;
    }

    ++report.checkedSnapshots;

    for (const auto& entry : commit.getFileHashes()) {
        const std::string& relativeText = entry.first;
        const std::string& expectedHash = entry.second;
        const fs::path relativePath(relativeText);

        if (!isSafeRelativePath(relativePath)) {
            addError(
                report,
                "path",
                commitId,
                "Unsafe snapshot path: " + relativeText
            );
            continue;
        }

        const fs::path snapshotFile = snapshotDirectory / relativePath;

        ec.clear();
        if (!fs::is_regular_file(snapshotFile, ec) || ec) {
            addError(
                report,
                "snapshot",
                relativeText,
                "Snapshot file is missing or is not regular."
            );
            continue;
        }

        const std::string actualHash =
            HashManager::calculateHash(snapshotFile);

        if (actualHash.empty()) {
            addError(
                report,
                "hash",
                relativeText,
                "Could not calculate snapshot file hash."
            );
            continue;
        }

        if (actualHash != expectedHash) {
            addError(
                report,
                "hash",
                relativeText,
                "Snapshot hash does not match commit metadata."
            );
        }
    }

    return true;
}

IntegrityReport IntegrityManager::verifyRepository() const {
    IntegrityReport report;
    std::error_code ec;

    Repository repository(repositoryRoot);
    if (!repository.isInitialized()) {
        addError(
            report,
            "repository",
            repositoryRoot.string(),
            "Directory is not an initialized MiniVCS repository."
        );
        return report;
    }

    const fs::path headPath = metadataPath / "HEAD";
    const fs::path branchesPath = metadataPath / "branches";

    std::ifstream headInput(headPath);
    std::string currentBranch;
    if (!headInput || !std::getline(headInput, currentBranch) ||
        currentBranch.empty()) {
        addError(
            report,
            "HEAD",
            headPath.string(),
            "HEAD is missing or empty."
        );
    }

    ec.clear();
    if (!fs::is_directory(branchesPath, ec) || ec) {
        addError(
            report,
            "branches",
            branchesPath.string(),
            "Branches directory is missing."
        );
        return report;
    }

    CommitManager commitManager(repositoryRoot);
    std::unordered_set<std::string> checkedCommits;
    std::unordered_set<std::string> branchNames;

    for (fs::directory_iterator it(branchesPath, ec), end;
         !ec && it != end;
         it.increment(ec)) {
        const fs::path branchPath = it->path();
        const std::string branchName = branchPath.filename().string();

        if (branchPath.extension() == ".tmp") {
            addWarning(
                report,
                "branch",
                branchPath.string(),
                "Temporary branch-pointer file exists."
            );
            continue;
        }

        if (!it->is_regular_file(ec) || ec) {
            ec.clear();
            continue;
        }

        branchNames.insert(branchName);
        ++report.checkedBranches;

        std::ifstream branchInput(branchPath);
        std::string commitId;
        std::getline(branchInput, commitId);

        if (commitId.empty()) {
            addWarning(
                report,
                "branch",
                branchName,
                "Branch has no commit yet."
            );
            continue;
        }

        Commit commit;
        if (!commitManager.getCommit(commitId, commit)) {
            addError(
                report,
                "branch",
                branchName,
                "Branch points to an unreadable commit: " + commitId
            );
            continue;
        }

        std::string currentId = commitId;
        std::unordered_set<std::string> history;

        while (!currentId.empty()) {
            if (!history.insert(currentId).second) {
                addError(
                    report,
                    "history",
                    branchName,
                    "Cycle detected in commit parent links."
                );
                break;
            }

            if (checkedCommits.insert(currentId).second) {
                verifyCommitSnapshot(currentId, report);
            }

            Commit currentCommit;
            if (!commitManager.getCommit(currentId, currentCommit)) {
                break;
            }

            currentId = currentCommit.getParentId();
        }
    }

    if (!currentBranch.empty() &&
        branchNames.find(currentBranch) == branchNames.end()) {
        addError(
            report,
            "HEAD",
            currentBranch,
            "HEAD refers to a branch that does not exist."
        );
    }

    if (ec) {
        addError(
            report,
            "branches",
            branchesPath.string(),
            "Could not finish reading the branches directory."
        );
    }

    return report;
}