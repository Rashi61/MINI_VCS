#include "BranchManager.h"

#include <stdexcept>

BranchManager::BranchManager(const std::string& initialCommitId)
    : currentBranch("main") {

    branches.emplace(
        "main",
        Branch("main", initialCommitId)
    );
}

bool BranchManager::createBranch(const std::string& branchName) {
    if (branchName.empty()) {
        return false;
    }

    if (branches.find(branchName) != branches.end()) {
        return false;
    }

    const Branch& current = branches.at(currentBranch);

    branches.emplace(
        branchName,
        Branch(branchName, current.getHeadCommit())
    );

    return true;
}

std::vector<std::string> BranchManager::listBranches() const {
    std::vector<std::string> result;

    for (const auto& entry : branches) {
        result.push_back(entry.first);
    }

    return result;
}

bool BranchManager::switchBranch(const std::string& branchName) {
    if (branches.find(branchName) == branches.end()) {
        return false;
    }

    currentBranch = branchName;
    return true;
}

bool BranchManager::deleteBranch(const std::string& branchName) {
    if (branchName == currentBranch) {
        return false;
    }

    auto it = branches.find(branchName);

    if (it == branches.end()) {
        return false;
    }

    branches.erase(it);
    return true;
}

std::string BranchManager::getCurrentBranch() const {
    return currentBranch;
}