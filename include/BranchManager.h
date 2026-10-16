#ifndef BRANCH_MANAGER_H
#define BRANCH_MANAGER_H

#include "Branch.h"

#include <map>
#include <string>
#include <vector>

class BranchManager {
private:
    std::map<std::string, Branch> branches;
    std::string currentBranch;

public:
    explicit BranchManager(const std::string& initialCommitId);

    bool createBranch(const std::string& branchName);
    std::vector<std::string> listBranches() const;
    bool switchBranch(const std::string& branchName);
    bool deleteBranch(const std::string& branchName);

    std::string getCurrentBranch() const;
};

#endif