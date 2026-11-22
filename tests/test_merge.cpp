#include "CommitManager.h"
#include "FileManager.h"
#include "MergeManager.h"
#include "branchmanager.h"
#include "repository.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <iterator>

namespace fs = std::filesystem;

namespace {

void writeFile(
    const fs::path& root,
    const std::string& relativePath,
    const std::string& contents
) {
    const fs::path filePath = root / relativePath;
    fs::create_directories(filePath.parent_path());

    std::ofstream file(filePath);
    assert(file);
    file << contents;
    assert(file.good());
}

void trackFile(
    const fs::path& root,
    const std::string& relativePath
) {
    FileManager fileManager(root);
    assert(fileManager.addFile(relativePath));
}

void createInitialCommit(
    const fs::path& root,
    const std::vector<std::string>& paths
) {
    for (const auto& path : paths) {
        trackFile(root, path);
    }

    CommitManager commitManager(root);
    assert(commitManager.createCommit("base commit"));
    assert(!commitManager.getLatestCommitId().empty());
}

void testCleanMerge() {
    const fs::path root =
        fs::temp_directory_path() / "minivcs_clean_merge_test";

    fs::remove_all(root);
    fs::create_directories(root);

    Repository repository(root);
    assert(repository.init());

    writeFile(root, "shared.txt", "base\n");
    writeFile(root, "main.txt", "base main\n");
    writeFile(root, "feature.txt", "base feature\n");

    createInitialCommit(
        root,
        {"shared.txt", "main.txt", "feature.txt"}
    );

    BranchManager branches(root);
    assert(branches.createBranch("feature"));

    const std::string baseCommit =
        branches.getCurrentCommit();

    writeFile(root, "main.txt", "main change\n");

    CommitManager commits(root);
    assert(commits.createCommit("main change"));

    const std::string mainCommit =
        branches.getCurrentCommit();

    assert(mainCommit != baseCommit);

    assert(branches.switchBranch("feature"));
    assert(branches.getCurrentCommit() == baseCommit);

    writeFile(root, "feature.txt", "feature change\n");
    assert(commits.createCommit("feature change"));

    const std::string featureCommit =
        branches.getCurrentCommit();

    assert(featureCommit != baseCommit);
    assert(featureCommit != mainCommit);

    assert(branches.switchBranch("main"));
    assert(branches.getCurrentCommit() == mainCommit);

    MergeManager mergeManager(root);
    const MergeResult result =
        mergeManager.mergeBranch("feature");

    assert(result.success);
    assert(!result.hasConflicts);
    assert(result.conflicts.empty());

    const std::string mergeCommitId =
        branches.getCurrentCommit();

    assert(!mergeCommitId.empty());
    assert(mergeCommitId != mainCommit);
    assert(mergeCommitId != featureCommit);

    Commit mergeCommit;
    assert(commits.getCommit(mergeCommitId, mergeCommit));

    assert(mergeCommit.getFirstParentId() == mainCommit);
    assert(mergeCommit.getSecondParentId() == featureCommit);
    assert(
        mergeCommit.getFirstParentId() !=
        mergeCommit.getSecondParentId()
    );

    fs::remove_all(root);
}

void testConflictingMerge() {
    const fs::path root =
        fs::temp_directory_path() / "minivcs_conflict_merge_test";

    fs::remove_all(root);
    fs::create_directories(root);

    Repository repository(root);
    assert(repository.init());

    writeFile(root, "conflict.txt", "base\n");

    createInitialCommit(root, {"conflict.txt"});

    BranchManager branches(root);
    assert(branches.createBranch("feature"));

    writeFile(root, "conflict.txt", "main version\n");

    CommitManager commits(root);
    assert(commits.createCommit("main conflicting change"));

    const std::string mainCommit =
        branches.getCurrentCommit();

    assert(branches.switchBranch("feature"));

    writeFile(root, "conflict.txt", "feature version\n");
    assert(commits.createCommit("feature conflicting change"));

    const std::string featureCommit =
        branches.getCurrentCommit();

    assert(featureCommit != mainCommit);

    assert(branches.switchBranch("main"));
    assert(branches.getCurrentCommit() == mainCommit);

    const std::string beforeMerge =
        branches.getCurrentCommit();

    MergeManager mergeManager(root);
    const MergeResult result =
        mergeManager.mergeBranch("feature");

    assert(!result.success);
    assert(result.hasConflicts);
    assert(!result.conflicts.empty());

    bool foundConflictPath = false;

    for (const auto& path : result.conflicts) {
        if (path == "conflict.txt") {
            foundConflictPath = true;
        }
    }

    assert(foundConflictPath);

        std::ifstream conflictedFile(
        root / "conflict.txt"
    );

    assert(conflictedFile);

    const std::string markerContents{
    std::istreambuf_iterator<char>(conflictedFile),
    std::istreambuf_iterator<char>()
};

    assert(
        markerContents.find("<<<<<<< ours") !=
        std::string::npos
    );

    assert(
        markerContents.find("=======") !=
        std::string::npos
    );

    assert(
        markerContents.find(">>>>>>> theirs") !=
        std::string::npos
    );

    // A failed/conflicting merge must not move the current branch pointer.
    assert(branches.getCurrentCommit() == beforeMerge);

    fs::remove_all(root);
}

void testMissingSourceBranch() {
    const fs::path root =
        fs::temp_directory_path() / "minivcs_missing_branch_test";

    fs::remove_all(root);
    fs::create_directories(root);

    Repository repository(root);
    assert(repository.init());

    writeFile(root, "file.txt", "initial\n");
    createInitialCommit(root, {"file.txt"});

    MergeManager mergeManager(root);
    const MergeResult result =
        mergeManager.mergeBranch("does-not-exist");

    assert(!result.success);
    assert(!result.hasConflicts);
    assert(result.conflicts.empty());

    fs::remove_all(root);
}

} // namespace

int main() {
    testCleanMerge();
    testConflictingMerge();
    testMissingSourceBranch();

    std::cout << "All merge integration tests passed\n";
    return 0;
}