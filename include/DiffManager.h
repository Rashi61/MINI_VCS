#ifndef DIFFMANAGER_H
#define DIFFMANAGER_H

#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

// Line-based text diffs between the working tree and commits, or between
// two commits. Reads only; never changes the repository.
class DiffManager {
private:
    std::filesystem::path repositoryRoot;

public:
    explicit DiffManager(const std::filesystem::path& rootPath);

    // Prints one commit's details (id, message, date, parent, files).
    bool showCommit(const std::string& commitId, std::ostream& out) const;

    // Working tree vs a commit (empty commitId = latest commit on HEAD).
    // Returns false on an error such as an unknown commit.
    bool diffWorkingTree(const std::string& commitId,
                         std::ostream& out) const;

    // Commit 'fromId' vs commit 'toId' (full IDs or unique prefixes).
    bool diffCommits(const std::string& fromId,
                     const std::string& toId,
                     std::ostream& out) const;

    // Unified-style diff of two sets of lines (3 lines of context).
    // Returns an empty string when the lines are identical. Sets
    // 'tooLarge' and returns "" if the files are too big to compare.
    static std::string diffLines(const std::vector<std::string>& oldLines,
                                 const std::vector<std::string>& newLines,
                                 bool& tooLarge);
};

#endif