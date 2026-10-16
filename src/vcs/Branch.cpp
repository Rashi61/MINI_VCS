
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
 
