#ifndef BRANCH_H
#define BRANCH_H

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

#endif