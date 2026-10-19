#include "branch.h"
#include "branchmanager.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <system_error>

Branch::Branch(const std::string& branchName,
               const std::string& commitId)
    : name(branchName), headCommit(commitId) {}

std::string Branch::getName() const {
    return name;
}

std::string Branch::getHeadCommit() const {
    return headCommit;
}

void Branch::setHeadCommit(const std::string& commitId) {
    headCommit = commitId;
}

BranchManager::BranchManager(
    const std::filesystem::path& repositoryRoot)
    : metadataPath(
          std::filesystem::absolute(repositoryRoot)
              .lexically_normal() / ".minivcs") {}

bool BranchManager::isValidBranchName(
    const std::string& branchName) const {
    if (branchName.empty() ||
        branchName == "." ||
        branchName == "..") {
        return false;
    }

    // NEW: reject names ending in ".tmp" (reserved for temp files)
    if (branchName.size() >= 4 &&
        branchName.compare(branchName.size() - 4, 4, ".tmp") == 0) {
        return false;
    }

    for (unsigned char c : branchName) {
        if (!std::isalnum(c) && c != '_' &&
            c != '-' && c != '.') {
            return false;
        }
    }

    return true;
}

std::string BranchManager::getCurrentBranch() const {
    std::ifstream head(metadataPath / "HEAD");
    std::string branchName;

    if (!head || !std::getline(head, branchName)) {
        return "";
    }

    return branchName;
}

std::vector<std::string> BranchManager::listBranches() const {
    std::vector<std::string> branches;
    std::error_code ec;

    const auto directory = metadataPath / "branches";

    if (!std::filesystem::is_directory(directory, ec) || ec) {
        return branches;
    }

    for (const auto& entry :
         std::filesystem::directory_iterator(directory, ec)) {
        if (ec) {
            break;
        }

        if (entry.is_regular_file(ec) && !ec &&
            entry.path().extension() != ".tmp") {
            branches.push_back(entry.path().filename().string());
        }

        ec.clear();
    }

    std::sort(branches.begin(), branches.end());
    return branches;
}

bool BranchManager::createBranch(
    const std::string& branchName) {
    if (!isValidBranchName(branchName)) {
        return false;
    }

    std::error_code ec;
    const auto branchesDir = metadataPath / "branches";

    if (!std::filesystem::is_directory(branchesDir, ec) || ec) {
        return false;
    }

    const auto branchPath = branchesDir / branchName;

    if (std::filesystem::exists(branchPath, ec) || ec) {
        return false;
    }

    std::ifstream head(metadataPath / "HEAD");
    std::string current;

    if (!head || !std::getline(head, current) ||
        !isValidBranchName(current)) {
        return false;
    }

    std::ifstream currentBranch(branchesDir / current);
    std::string commitId;

    if (!currentBranch || !std::getline(currentBranch, commitId)) {
        return false;
    }

    const auto tempPath = branchesDir / (branchName + ".tmp");

    {
        std::ofstream output(tempPath);
        if (!output) {
            return false;
        }

        output << commitId << '\n';
        output.close();

        if (!output) {
            std::filesystem::remove(tempPath, ec);
            return false;
        }
    }

    std::filesystem::rename(tempPath, branchPath, ec);

    if (ec) {
        std::filesystem::remove(tempPath, ec);
        return false;
    }

    return true;
}

bool BranchManager::switchBranch(
    const std::string& branchName) {
    if (!isValidBranchName(branchName)) {
        return false;
    }

    std::error_code ec;
    const auto branchPath = metadataPath / "branches" / branchName;

    if (!std::filesystem::is_regular_file(branchPath, ec) || ec) {
        return false;
    }

    const auto headPath = metadataPath / "HEAD";
    const auto tempPath = metadataPath / "HEAD.tmp";

    {
        std::ofstream output(tempPath);
        if (!output) {
            return false;
        }

        output << branchName << '\n';
        output.close();

        if (!output) {
            std::filesystem::remove(tempPath, ec);
            return false;
        }
    }

    std::filesystem::rename(tempPath, headPath, ec);

    if (ec) {
        std::filesystem::remove(tempPath, ec);
        return false;
    }

    return true;
}

bool BranchManager::deleteBranch(
    const std::string& branchName) {
    if (!isValidBranchName(branchName) ||
        branchName == "main" ||
        branchName == getCurrentBranch()) {
        return false;
    }

    std::error_code ec;
    const auto branchPath = metadataPath / "branches" / branchName;

    if (!std::filesystem::is_regular_file(branchPath, ec) || ec) {
        return false;
    }

    return std::filesystem::remove(branchPath, ec) && !ec;
}
bool BranchManager::branchExists(const std::string& branchName) const {
    if (!isValidBranchName(branchName)) return false;
    std::error_code ec;
    return std::filesystem::is_regular_file(
               metadataPath / "branches" / branchName, ec) && !ec;
}

std::string BranchManager::getBranchCommit(
    const std::string& branchName) const {
    if (!isValidBranchName(branchName)) return "";
    std::ifstream in(metadataPath / "branches" / branchName);
    std::string id;
    if (!in || !std::getline(in, id)) return "";
    return id;
}

std::string BranchManager::getCurrentCommit() const {
    return getBranchCommit(getCurrentBranch());
}

