
#include "Commit.h"

Commit::Commit(
    const std::string& commitId,
    const std::string& commitMessage,
    const std::string& commitTimestamp,
    const std::string& previousCommitId,
    const std::unordered_map<std::string, std::string>& hashes
)
    : id(commitId),
      message(commitMessage),
      timestamp(commitTimestamp),
      parentId(previousCommitId),
      fileHashes(hashes) {
}

const std::string& Commit::getId() const {
    return id;
}

const std::string& Commit::getMessage() const {
    return message;
}

const std::string& Commit::getTimestamp() const {
    return timestamp;
}

const std::string& Commit::getParentId() const {
    return parentId;
}

const std::unordered_map<std::string, std::string>& Commit::getFileHashes() const {
    return fileHashes;
}
