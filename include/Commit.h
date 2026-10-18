#ifndef COMMIT_H
#define COMMIT_H

#include <string>
#include <unordered_map>

class Commit {
private:
    std::string id;
    std::string message;
    std::string timestamp;
    std::string parentId;
    std::unordered_map<std::string, std::string> fileHashes;

public:
    Commit() = default;

    Commit(
        const std::string& commitId,
        const std::string& commitMessage,
        const std::string& commitTimestamp,
        const std::string& previousCommitId,
        const std::unordered_map<std::string, std::string>& hashes
    );

    const std::string& getId() const;
    const std::string& getMessage() const;
    const std::string& getTimestamp() const;
    const std::string& getParentId() const;

    const std::unordered_map<std::string, std::string>&
    getFileHashes() const;
};

#endif
