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
      authorId("unknown"),
      firstParentId(previousCommitId),
      secondParentId(""),
      fileHashes(hashes) {
}

Commit::Commit(
    const std::string& commitId,
    const std::string& commitMessage,
    const std::string& commitTimestamp,
    const std::string& commitAuthorId,
    const std::string& firstParent,
    const std::string& secondParent,
    const std::unordered_map<std::string, std::string>& hashes
)
    : id(commitId),
      message(commitMessage),
      timestamp(commitTimestamp),
      authorId(commitAuthorId),
      firstParentId(firstParent),
      secondParentId(secondParent),
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

const std::string& Commit::getAuthorId() const {
    return authorId;
}

const std::string& Commit::getFirstParentId() const {
    return firstParentId;
}

const std::string& Commit::getSecondParentId() const {
    return secondParentId;
}

const std::string& Commit::getParentId() const {
    return firstParentId;
}

const std::unordered_map<std::string, std::string>&
Commit::getFileHashes() const {
    return fileHashes;
}
