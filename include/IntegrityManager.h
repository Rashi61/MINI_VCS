#ifndef INTEGRITY_MANAGER_H
#define INTEGRITY_MANAGER_H

#include <filesystem>
#include <string>
#include <vector>

struct IntegrityIssue {
    bool warning = false;
    std::string category;
    std::string path;
    std::string message;
};

struct IntegrityReport {
    bool valid = true;
    int checkedBranches = 0;
    int checkedCommits = 0;
    int checkedSnapshots = 0;
    std::vector<IntegrityIssue> issues;
};

class IntegrityManager {
private:
    std::filesystem::path repositoryRoot;
    std::filesystem::path metadataPath;
    std::filesystem::path objectsPath;

    void addError(
        IntegrityReport& report,
        const std::string& category,
        const std::string& path,
        const std::string& message
    ) const;

    void addWarning(
        IntegrityReport& report,
        const std::string& category,
        const std::string& path,
        const std::string& message
    ) const;

    bool isSafeRelativePath(
        const std::filesystem::path& relative
    ) const;

    bool verifyCommitSnapshot(
        const std::string& commitId,
        IntegrityReport& report
    ) const;

public:
    explicit IntegrityManager(
        const std::filesystem::path& rootPath
    );

    IntegrityReport verifyRepository() const;
};

#endif