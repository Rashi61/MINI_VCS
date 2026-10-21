#ifndef DIFFMANAGER_H
#define DIFFMANAGER_H

#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

class DiffManager {
private:
    std::filesystem::path repositoryRoot;

public:
    explicit DiffManager(const std::filesystem::path& rootPath);

    bool showCommit(const std::string& commitId, std::ostream& out) const;

    bool diffWorkingTree(const std::string& commitId,
                         std::ostream& out) const;


    bool diffCommits(const std::string& fromId,
                     const std::string& toId,
                     std::ostream& out) const;

    static std::string diffLines(const std::vector<std::string>& oldLines,
                                 const std::vector<std::string>& newLines,
                                 bool& tooLarge);
};

#endif