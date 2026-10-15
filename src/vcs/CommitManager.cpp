#include "CommitManager.h"
#include "FileManager.h"
#include "HashManager.h"
#include <iostream>
#include <algorithm>
#include <system_error>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>

CommitManager::CommitManager(const std::filesystem::path& rootPath)
    : repositoryRoot(std::filesystem::absolute(rootPath)),
      metadataPath(repositoryRoot / ".minivcs"),
      objectsPath(metadataPath / "objects") {}

std::string CommitManager::getLatestCommitId() const {
    std::ifstream headFile(metadataPath / "HEAD");
    std::string branchName;

    if (!headFile || !std::getline(headFile, branchName) ||
        branchName.empty()) {
        return "";
    }

    std::ifstream branchFile(metadataPath / "branches" / branchName);
    std::string commitId;

    if (!branchFile || !std::getline(branchFile, commitId)) {
        return "";
    }

    return commitId;
}


bool CommitManager::createCommit(const std::string& message) {
    if (message.empty()) {
        return false;
    }
    std::ifstream headFile(metadataPath / "HEAD");
    std::string branchName;

    if (!headFile ||
        !std::getline(headFile, branchName) ||
        branchName.empty() ||
        branchName.find_first_of("/\\") != std::string::npos ||
        branchName == "." ||
        branchName == "..") {
        return false;
    }
    headFile.close();

    const auto branchPath = metadataPath / "branches" / branchName;
    std::error_code ec;

    if (!std::filesystem::is_regular_file(branchPath, ec) || ec) {
        return false;
    }

    std::ifstream branchFile(branchPath);
    std::string parentId;

    if (!branchFile || !std::getline(branchFile, parentId)) {
        return false;
    }
    branchFile.close();

    if (!parentId.empty() &&
        !std::filesystem::is_regular_file(
            objectsPath / parentId / "commit.txt", ec)) {
        return false;
    }

    FileManager fileManager(repositoryRoot);
    const auto files = fileManager.getTrackedFiles();

    if (files.empty()) {
        std::cerr << "Nothing to commit: no tracked files.\n";
        return false;
    }

    std::unordered_map<std::string, std::string> hashes;

    for (const auto& relativePath : files) {
        const auto filePath = repositoryRoot / relativePath;

        ec.clear();
        if (!std::filesystem::is_regular_file(filePath, ec) || ec) {
            std::cerr << "Cannot commit missing file: "
                      << relativePath << '\n';
            return false;
        }

        const std::string hash = HashManager::calculateHash(filePath);

        if (hash.empty()) {
            std::cerr << "Could not hash file: "
                      << relativePath << '\n';
            return false;
        }

        hashes[relativePath] = hash;
    }

    if (!parentId.empty()) {
        const auto history = getCommitHistory();

        if (history.empty() || history.back().getId() != parentId) {
            std::cerr << "Could not read the parent commit.\n";
            return false;
        }

        if (hashes == history.back().getFileHashes()) {
            std::cout << "Nothing to commit.\n";
            return false;
        }
    }

    const auto now = std::chrono::system_clock::now();

    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

    const std::string commitId = std::to_string(milliseconds);

    std::time_t currentTime =
        std::chrono::system_clock::to_time_t(now);

    std::tm timeInfo{};

#ifdef _WIN32
    localtime_s(&timeInfo, &currentTime);
#else
    localtime_r(&currentTime, &timeInfo);
#endif

    std::ostringstream timestampStream;
    timestampStream << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S");

    const auto finalDir = objectsPath / commitId;
    const auto tempDir = objectsPath / (commitId + ".tmp");

    std::filesystem::create_directories(objectsPath, ec);

    if (ec || std::filesystem::exists(finalDir, ec) || ec) {
        return false;
    }

    std::filesystem::remove_all(tempDir, ec);
    ec.clear();

    if (!std::filesystem::create_directories(tempDir / "files", ec) ||
        ec) {
        return false;
    }

    for (const auto& relativePath : files) {
        const auto source = repositoryRoot / relativePath;
        const auto destination = tempDir / "files" / relativePath;

        std::filesystem::create_directories(
            destination.parent_path(), ec);

        if (ec) {
            std::filesystem::remove_all(tempDir);
            return false;
        }

        std::filesystem::copy_file(
            source, destination,
            std::filesystem::copy_options::overwrite_existing, ec);

        if (ec) {
            std::filesystem::remove_all(tempDir);
            return false;
        }
    }

    std::ofstream commitFile(tempDir / "commit.txt");

    if (!commitFile) {
        std::filesystem::remove_all(tempDir);
        return false;
    }

    commitFile << std::quoted(commitId) << '\n'
               << std::quoted(message) << '\n'
               << std::quoted(timestampStream.str()) << '\n'
               << std::quoted(parentId) << '\n'
               << hashes.size() << '\n';

    // Sort paths for deterministic commit metadata.
    std::vector<std::string> sortedPaths;
    sortedPaths.reserve(hashes.size());

    for (const auto& entry : hashes) {
        sortedPaths.push_back(entry.first);
    }

    std::sort(sortedPaths.begin(), sortedPaths.end());

    for (const auto& path : sortedPaths) {
        commitFile << std::quoted(path) << '\n'
                   << std::quoted(hashes.at(path)) << '\n';
    }

    commitFile.close();

    if (!commitFile) {
        std::filesystem::remove_all(tempDir);
        return false;
    }

    std::filesystem::rename(tempDir, finalDir, ec);

    if (ec) {
        std::filesystem::remove_all(tempDir);
        return false;
    }

    // Replace the branch pointer using a temporary file.
    const auto branchTempPath =
        metadataPath / "branches" / (branchName + ".tmp");

    {
        std::ofstream branchTemp(branchTempPath);

        if (!branchTemp) {
            std::filesystem::remove_all(finalDir);
            return false;
        }

        branchTemp << commitId << '\n';
        branchTemp.close();

        if (!branchTemp) {
            std::filesystem::remove(branchTempPath, ec);
            std::filesystem::remove_all(finalDir);
            return false;
        }
    }

    std::filesystem::rename(branchTempPath, branchPath, ec);

    if (ec) {
        std::error_code cleanupError;
        std::filesystem::remove(branchTempPath, cleanupError);
        std::filesystem::remove_all(finalDir, cleanupError);
        return false;
    }

    return true;
}

std::vector<Commit> CommitManager::getCommitHistory() const {
    std::vector<Commit> history;
    std::string commitId = getLatestCommitId();

    while (!commitId.empty()) {
        std::ifstream commitFile(
            objectsPath / commitId / "commit.txt");

        if (!commitFile) {
            break;
        }

        std::string storedId;
        std::string message;
        std::string timestamp;
        std::string parentId;
        std::size_t fileCount = 0;

        if (!(commitFile >> std::quoted(storedId)
                        >> std::quoted(message)
                        >> std::quoted(timestamp)
                        >> std::quoted(parentId)
                        >> fileCount)) {
            break;
        }

        std::unordered_map<std::string, std::string> hashes;

        for (std::size_t i = 0; i < fileCount; ++i) {
            std::string path;
            std::string hash;

            if (!(commitFile >> std::quoted(path)
                            >> std::quoted(hash))) {
                hashes.clear();
                break;
            }

            hashes[path] = hash;
        }

        if (hashes.size() != fileCount) {
            break;
        }

        history.emplace_back(
            storedId, message, timestamp, parentId, hashes);

        commitId = parentId;
    }

    std::reverse(history.begin(), history.end());
    return history;
}
