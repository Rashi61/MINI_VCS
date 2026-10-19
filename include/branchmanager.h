#ifndef BRANCH_MANAGER_H
#define BRANCH_MANAGER_H

#include <filesystem>
#include <string>
#include <vector>

class BranchManager {
private:
    std::filesystem::path metadataPath;

    bool isValidBranchName(const std::string& branchName) const;

public:
    explicit BranchManager(
        const std::filesystem::path& repositoryRoot
    );

    bool createBranch(const std::string& branchName);
    std::vector<std::string> listBranches() const;
    bool switchBranch(const std::string& branchName);
    bool deleteBranch(const std::string& branchName);

    std::string getCurrentBranch() const;
    bool branchExists(const std::string& branchName) const;
std::string getBranchCommit(const std::string& branchName) const;
std::string getCurrentCommit() const;
bool updateBranchCommit(const std::string& branchName,
                        const std::string& commitId);
};

#endif
