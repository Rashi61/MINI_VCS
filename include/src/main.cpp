#pragma once
#include <iostream>
#include "Branch.h"
#include <string>

class Branch {
private:
    std::string name;
    std::string headCommit;

public:
    Branch(const std::string& branchName,
           const std::string& commitId);

    std::string getName() const;

    std::string getHeadCommit() const;

    void setHeadCommit(const std::string& commitId);
};
#include "Branch.h"

Branch::Branch(const std::string& branchName,
               const std::string& commitId)
    : name(branchName), headCommit(commitId) {
}

std::string Branch::getName() const {
    return name;
}

std::string Branch::getHeadCommit() const {
    return headCommit;
}

void Branch::setHeadCommit(const std::string& commitId) {
    headCommit = commitId;
}

int main() {
    Branch mainBranch("main", "C101");

    std::cout << "Branch name: "
              << mainBranch.getName() << '\n';

    std::cout << "Current commit: "
              << mainBranch.getHeadCommit() << '\n';

    mainBranch.setHeadCommit("C102");

    std::cout << "Updated commit: "
              << mainBranch.getHeadCommit() << '\n';

    return 0;
}