#include "CommitManager.h"
#include "FileManager.h"
#include "HashManager.h"

#include <algorithm>
#include <exception>
#include <cctype>
#include <system_error>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

CommitManager::CommitManager(const std::filesystem::path& rootPath)
    : repositoryRoot(std::filesystem::absolute(rootPath)),
      metadataPath(repositoryRoot / ".minivcs"),
      objectsPath(metadataPath / "objects") {}

std::string CommitManager::getLatestCommitId() const {
    std::ifstream headFile(metadataPath / "HEAD");
    std::string branchName;

    if (!headFile || !std::getline(headFile, branchName) ||
        branchName.empty() ||
        branchName.find_first_of("/\\") != std::string::npos ||
        branchName == "." || branchName == "..") {
        return "";
    }

    std::ifstream branchFile(
        metadataPath / "branches" / branchName);

    std::string commitId;

    if (!branchFile || !std::getline(branchFile, commitId)) {
        return "";
    }

    return commitId;
}

bool CommitManager::createCommit(const std::string& message) {
    if (message.empty()) {
        std::cerr << "Commit message cannot be empty.\n";
        return false;
    }


    std::ifstream headFile(metadataPath / "HEAD");
    std::string branchName;

    if (!headFile || !std::getline(headFile, branchName) ||
        branchName.empty() ||
        branchName.find_first_of("/\\") != std::string::npos ||
        branchName == "." || branchName == "..") {
        std::cerr << "Could not read HEAD.\n";
        return false;
    }

    headFile.close();

    const auto branchPath =
        metadataPath / "branches" / branchName;

    std::error_code ec;

    if (!std::filesystem::is_regular_file(branchPath, ec) || ec) {
        std::cerr << "Current branch does not exist.\n";
        return false;
    }

    std::ifstream branchFile(branchPath);
    std::string parentId;

    if (!branchFile || !std::getline(branchFile, parentId)) {
        std::cerr << "Could not read the current branch.\n";
        return false;
    }

    branchFile.close();

    if (!parentId.empty() &&
        !std::filesystem::is_regular_file(
            objectsPath / parentId / "commit.txt", ec)) {
        std::cerr << "Parent commit does not exist.\n";
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

        const std::string hash =
            HashManager::calculateHash(filePath);

        if (hash.empty()) {
            std::cerr << "Could not hash file: "
                      << relativePath << '\n';
            return false;
        }

        hashes[relativePath] = hash;
    }

    if (!parentId.empty()) {
        const auto history = getCommitHistory();

        if (history.empty() ||
            history.back().getId() != parentId) {
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

    const std::string timestampForId =
        std::to_string(milliseconds);

    const std::time_t currentTime =
        std::chrono::system_clock::to_time_t(now);

    std::tm timeInfo{};

#ifdef _WIN32
    localtime_s(&timeInfo, &currentTime);
#else
    localtime_r(&currentTime, &timeInfo);
#endif

    std::ostringstream timestampStream;
    timestampStream << std::put_time(
        &timeInfo, "%Y-%m-%d %H:%M:%S");

    const std::string timestamp = timestampStream.str();

    std::vector<std::string> sortedPaths;
    sortedPaths.reserve(hashes.size());

    for (const auto& entry : hashes) {
        sortedPaths.push_back(entry.first);
    }

    std::sort(sortedPaths.begin(), sortedPaths.end());

    std::ostringstream idInput;

    idInput << parentId << '\n'
            << message << '\n'
            << timestampForId << '\n';

    for (const auto& path : sortedPaths) {
        idInput << path << ':' << hashes.at(path) << '\n';
    }

    const std::string commitId =
        HashManager::calculateHashFromString(idInput.str());

    if (commitId.empty()) {
        std::cerr << "Could not calculate commit ID.\n";
        return false;
    }

    const auto finalDir = objectsPath / commitId;
    const auto tempDir = objectsPath / (commitId + ".tmp");

    std::filesystem::create_directories(objectsPath, ec);

    if (ec || std::filesystem::exists(finalDir, ec) || ec) {
        std::cerr << "Could not prepare commit directory.\n";
        return false;
    }

    std::filesystem::remove_all(tempDir, ec);
    ec.clear();

    if (!std::filesystem::create_directories(
            tempDir / "files", ec) || ec) {
        std::filesystem::remove_all(tempDir);
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
            source,
            destination,
            std::filesystem::copy_options::overwrite_existing,
            ec);

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
               << std::quoted(timestamp) << '\n'
               << std::quoted(parentId) << '\n'
               << hashes.size() << '\n';

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

        std::cerr
            << "Could not update branch pointer. "
            << "The previous branch pointer is unchanged.\n";
        return false;
    }

    std::cout << "Commit created: "
              << commitId.substr(0, 8) << '\n';

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





bool CommitManager::restoreCommit(const std::string& commitId) {
    namespace fs = std::filesystem;

    // Only accept simple, portable commit IDs. Existing numeric IDs and
    // hexadecimal SHA-256 IDs are supported.
    if (commitId.empty() ||
        !std::all_of(commitId.begin(), commitId.end(),
            [](unsigned char c) {
                return std::isalnum(c) || c == '-' || c == '_';
            })) {
        std::cerr << "Invalid commit ID.\n";
        return false;
    }

    const fs::path commitDir = objectsPath / commitId;
    const fs::path commitFilePath = commitDir / "commit.txt";
    const fs::path snapshotDir = commitDir / "files";
    const fs::path metadataDir = repositoryRoot / ".minivcs";
    const fs::path stagingDir = metadataDir / "restore-staging";

    std::error_code ec;

    auto isMissingError = [](const std::error_code& error) {
        return error == std::errc::no_such_file_or_directory ||
               error == std::errc::not_a_directory;
    };

    auto lower = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
        return value;
    };

    auto isSha256 = [](const std::string& hash) {
        return hash.size() == 64 &&
            std::all_of(hash.begin(), hash.end(),
                [](unsigned char c) {
                    return std::isxdigit(c) != 0;
                });
    };

    ec.clear();
    if (!fs::is_regular_file(commitFilePath, ec) || ec) {
        std::cerr << "Commit does not exist.\n";
        return false;
    }

    std::ifstream commitFile(commitFilePath);
    if (!commitFile) {
        std::cerr << "Could not open commit metadata.\n";
        return false;
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
                     >> fileCount) ||
        storedId != commitId ||
        fileCount > 100000) {
        std::cerr << "Invalid commit metadata.\n";
        return false;
    }

    std::vector<std::string> paths;
    std::unordered_map<std::string, std::string> hashes;
    std::unordered_set<std::string> foldedPaths;

    for (std::size_t i = 0; i < fileCount; ++i) {
        std::string storedPath;
        std::string hash;

        if (!(commitFile >> std::quoted(storedPath)
                         >> std::quoted(hash))) {
            std::cerr << "Incomplete commit metadata.\n";
            return false;
        }

        if (!isSha256(hash)) {
            std::cerr << "Invalid file hash in commit metadata.\n";
            return false;
        }

        // Interpret both slash styles as separators. Reject Windows drive
        // syntax, alternate data streams, and forbidden Windows characters.
        std::replace(storedPath.begin(), storedPath.end(), '\\', '/');

        if (storedPath.empty() ||
            storedPath.front() == '/' ||
            storedPath.find(':') != std::string::npos ||
            storedPath.find('<') != std::string::npos ||
            storedPath.find('>') != std::string::npos ||
            storedPath.find('"') != std::string::npos ||
            storedPath.find('|') != std::string::npos ||
            storedPath.find('?') != std::string::npos ||
            storedPath.find('*') != std::string::npos) {
            std::cerr << "Unsafe path in commit metadata.\n";
            return false;
        }

        fs::path relativePath(storedPath);

        if (relativePath.is_absolute() ||
            relativePath.has_root_name() ||
            relativePath.has_root_directory()) {
            std::cerr << "Unsafe path in commit metadata.\n";
            return false;
        }

        relativePath = relativePath.lexically_normal();

        if (relativePath.empty() || relativePath == ".") {
            std::cerr << "Invalid snapshot path.\n";
            return false;
        }

        for (const auto& component : relativePath) {
            const std::string name = component.string();
            const std::string folded = lower(name);

            if (name.empty() || name == "." || name == ".." ||
                name.back() == '.' || name.back() == ' ') {
                std::cerr << "Unsafe path in commit metadata.\n";
                return false;
            }

            // Protect the repository metadata directory on Windows,
            // regardless of capitalization.
            if (folded == ".minivcs") {
                std::cerr << "Snapshot cannot target .minivcs.\n";
                return false;
            }

            // Reject Windows device names, including names with extensions.
            std::string deviceBase = folded.substr(0, folded.find('.'));
            if (deviceBase == "con" || deviceBase == "prn" ||
                deviceBase == "aux" || deviceBase == "nul") {
                std::cerr << "Reserved Windows path in snapshot.\n";
                return false;
            }

            if (deviceBase.size() == 4 &&
                (deviceBase.compare(0, 3, "com") == 0 ||
                 deviceBase.compare(0, 3, "lpt") == 0) &&
                deviceBase[3] >= '1' && deviceBase[3] <= '9') {
                std::cerr << "Reserved Windows path in snapshot.\n";
                return false;
            }
        }

        const std::string normalized = relativePath.generic_string();
        const std::string folded = lower(normalized);

        if (!hashes.emplace(normalized, hash).second ||
            !foldedPaths.emplace(folded).second) {
            std::cerr << "Duplicate or case-conflicting snapshot path.\n";
            return false;
        }

        paths.push_back(normalized);
    }

    std::string extra;
    if (commitFile >> extra) {
        std::cerr << "Unexpected data in commit metadata.\n";
        return false;
    }

    ec.clear();
    if (!fs::is_directory(metadataDir, ec) || ec) {
        std::cerr << "Repository metadata directory is missing.\n";
        return false;
    }

    ec.clear();
    if (!fs::is_directory(snapshotDir, ec) || ec) {
        std::cerr << "Commit snapshot is missing.\n";
        return false;
    }

    // A snapshot cannot contain a file and another file below that path.
    for (const auto& path : paths) {
        fs::path parent = fs::path(path).parent_path();

        while (!parent.empty() && parent != ".") {
            if (foldedPaths.count(lower(parent.generic_string())) != 0) {
                std::cerr << "Conflicting paths in snapshot.\n";
                return false;
            }
            parent = parent.parent_path();
        }
    }

    // Check for symlinks in a path. Snapshot data and working-tree paths
    // must not use symlinks to redirect file operations elsewhere.
    auto containsSymlink = [&](const fs::path& base,
                               const fs::path& relative) {
        fs::path current = base;

        for (const auto& component : relative) {
            current /= component;
            std::error_code linkError;
            const fs::file_status status =
                fs::symlink_status(current, linkError);

            if (linkError) {
                if (isMissingError(linkError)) {
                    continue;
                }
                return true;
            }

            if (fs::is_symlink(status)) {
                return true;
            }
        }

        return false;
    };

    // Verify every snapshot file before touching the working tree.
    for (const auto& path : paths) {
        const fs::path relative(path);
        const fs::path source = snapshotDir / relative;

        if (containsSymlink(snapshotDir, relative)) {
            std::cerr << "Symlink found in snapshot path: "
                      << path << '\n';
            return false;
        }

        ec.clear();
        const fs::file_status status = fs::symlink_status(source, ec);

        if (ec || !fs::is_regular_file(status)) {
            std::cerr << "Snapshot file is missing or not regular: "
                      << path << '\n';
            return false;
        }

        try {
            if (HashManager::calculateHash(source) != hashes.at(path)) {
                std::cerr << "Snapshot file is corrupted: "
                          << path << '\n';
                return false;
            }
        } catch (const std::exception& e) {
            std::cerr << "Could not verify snapshot file "
                      << path << ": " << e.what() << '\n';
            return false;
        }
    }

    // Never overwrite an existing staging directory. It may contain
    // backups left behind after a previous incomplete rollback.
    ec.clear();
    const fs::file_status stagingStatus =
        fs::symlink_status(stagingDir, ec);

    if (!ec && fs::exists(stagingStatus)) {
        std::cerr << "Restore staging directory already exists. "
                  << "Inspect and preserve it before retrying: "
                  << stagingDir.string() << '\n';
        return false;
    }

    if (ec && !isMissingError(ec)) {
        std::cerr << "Could not inspect restore staging directory.\n";
        return false;
    }

    ec.clear();
    if (!fs::create_directory(stagingDir, ec) || ec) {
        std::cerr << "Could not create restore staging directory.\n";
        return false;
    }

    const fs::path stagedFiles = stagingDir / "files";
    const fs::path backups = stagingDir / "backups";

    auto cleanupStaging = [&]() {
        std::error_code cleanupError;
        fs::remove_all(stagingDir, cleanupError);
        return !cleanupError;
    };

    ec.clear();
    fs::create_directories(stagedFiles, ec);
    if (ec) {
        cleanupStaging();
        std::cerr << "Could not prepare staged files.\n";
        return false;
    }

    ec.clear();
    fs::create_directories(backups, ec);
    if (ec) {
        cleanupStaging();
        std::cerr << "Could not prepare restore backups.\n";
        return false;
    }

    // Copy and verify all files in staging before changing destinations.
    for (const auto& path : paths) {
        const fs::path relative(path);
        const fs::path source = snapshotDir / relative;
        const fs::path staged = stagedFiles / relative;

        ec.clear();
        fs::create_directories(staged.parent_path(), ec);
        if (ec) {
            cleanupStaging();
            std::cerr << "Could not prepare staged path: "
                      << path << '\n';
            return false;
        }

        ec.clear();
        fs::copy_file(source, staged,
                      fs::copy_options::overwrite_existing, ec);
        if (ec) {
            cleanupStaging();
            std::cerr << "Could not stage file: " << path << '\n';
            return false;
        }

        try {
            if (HashManager::calculateHash(staged) != hashes.at(path)) {
                cleanupStaging();
                std::cerr << "Staged file failed verification: "
                          << path << '\n';
                return false;
            }
        } catch (const std::exception& e) {
            cleanupStaging();
            std::cerr << "Could not verify staged file "
                      << path << ": " << e.what() << '\n';
            return false;
        }
    }

    // Preflight the complete destination layout before making changes.
    // Existing directories cannot be overwritten by snapshot files;
    // existing files cannot be used as parent directories.
    for (const auto& path : paths) {
        const fs::path relative(path);
        const fs::path destination = repositoryRoot / relative;

        if (containsSymlink(repositoryRoot, relative)) {
            cleanupStaging();
            std::cerr << "Refusing to restore through a symbolic link: "
                      << path << '\n';
            return false;
        }

        fs::path current = repositoryRoot;
        fs::path parent = relative.parent_path();

        for (const auto& component : parent) {
            current /= component;
            ec.clear();
            const fs::file_status status = fs::symlink_status(current, ec);

            if (!ec && fs::exists(status) &&
                !fs::is_directory(status)) {
                cleanupStaging();
                std::cerr << "Parent path is not a directory: "
                          << current.string() << '\n';
                return false;
            }

            if (ec && !isMissingError(ec)) {
                cleanupStaging();
                std::cerr << "Could not inspect parent path: "
                          << current.string() << '\n';
                return false;
            }
        }

        ec.clear();
        const fs::file_status status =
            fs::symlink_status(destination, ec);

        if (!ec && fs::exists(status) &&
            !fs::is_regular_file(status)) {
            cleanupStaging();
            std::cerr << "Destination is not a regular file: "
                      << path << '\n';
            return false;
        }

        if (ec && !isMissingError(ec)) {
            cleanupStaging();
            std::cerr << "Could not inspect destination: "
                      << path << '\n';
            return false;
        }
    }

    // Back up every existing destination before applying any changes.
    std::unordered_set<std::string> backedUpPaths;

    for (const auto& path : paths) {
        const fs::path relative(path);
        const fs::path destination = repositoryRoot / relative;

        ec.clear();
        const fs::file_status status =
            fs::symlink_status(destination, ec);

        if (!ec && fs::exists(status)) {
            const fs::path backup = backups / relative;

            ec.clear();
            fs::create_directories(backup.parent_path(), ec);
            if (ec) {
                cleanupStaging();
                std::cerr << "Could not prepare backup for: "
                          << path << '\n';
                return false;
            }

            ec.clear();
            fs::copy_file(destination, backup,
                          fs::copy_options::overwrite_existing, ec);
            if (ec) {
                cleanupStaging();
                std::cerr << "Could not back up destination: "
                          << path << '\n';
                return false;
            }

            backedUpPaths.insert(path);
        } else if (ec && !isMissingError(ec)) {
            cleanupStaging();
            std::cerr << "Could not inspect destination for backup: "
                      << path << '\n';
            return false;
        }
    }

    std::vector<std::string> attemptedPaths;
    std::vector<fs::path> createdDirectories;
    bool restoreSucceeded = true;

    // Create parent directories one component at a time so that each
    // directory created by this restore can be removed during rollback.
    auto ensureParentDirectories = [&](const fs::path& relative) {
        fs::path current = repositoryRoot;

        for (const auto& component : relative.parent_path()) {
            current /= component;
            ec.clear();

            const fs::file_status status = fs::symlink_status(current, ec);

            if (!ec && fs::exists(status)) {
                if (fs::is_symlink(status) ||
                    !fs::is_directory(status)) {
                    return false;
                }
                continue;
            }

            if (ec && !isMissingError(ec)) {
                return false;
            }

            ec.clear();
            if (!fs::create_directory(current, ec) || ec) {
                return false;
            }

            createdDirectories.push_back(current);
        }

        return true;
    };

    for (const auto& path : paths) {
        const fs::path relative(path);
        const fs::path staged = stagedFiles / relative;
        const fs::path destination = repositoryRoot / relative;

        if (!ensureParentDirectories(relative)) {
            std::cerr << "Could not prepare destination directories for: "
                      << path << '\n';
            restoreSucceeded = false;
            break;
        }

        // Record the path before copying because a failed copy may have
        // partially changed the destination.
        attemptedPaths.push_back(path);

        ec.clear();
        fs::copy_file(staged, destination,
                      fs::copy_options::overwrite_existing, ec);

        if (ec) {
            std::cerr << "Could not restore: " << path << '\n';
            restoreSucceeded = false;
            break;
        }

        try {
            if (HashManager::calculateHash(destination) != hashes.at(path)) {
                std::cerr << "Restored file failed verification: "
                          << path << '\n';
                restoreSucceeded = false;
                break;
            }
        } catch (const std::exception& e) {
            std::cerr << "Could not verify restored file "
                      << path << ": " << e.what() << '\n';
            restoreSucceeded = false;
            break;
        }
    }

    if (!restoreSucceeded) {
        bool rollbackSucceeded = true;

        // Roll back in reverse application order.
        for (auto it = attemptedPaths.rbegin();
             it != attemptedPaths.rend(); ++it) {
            const std::string& path = *it;
            const fs::path destination = repositoryRoot / path;

            ec.clear();

            if (backedUpPaths.count(path) != 0) {
                const fs::path backup = backups / path;

                fs::copy_file(backup, destination,
                              fs::copy_options::overwrite_existing, ec);

                if (ec) {
                    rollbackSucceeded = false;
                    std::cerr << "ROLLBACK FAILED for: "
                              << path << '\n';
                }
            } else {
                fs::remove(destination, ec);

                if (ec) {
                    rollbackSucceeded = false;
                    std::cerr << "Could not remove partially restored file: "
                              << path << '\n';
                }
            }
        }

        // Remove only directories created by this attempt, deepest first.
        for (auto it = createdDirectories.rbegin();
             it != createdDirectories.rend(); ++it) {
            ec.clear();
            fs::remove(*it, ec); // Fails harmlessly if the directory isn't empty.

            if (ec) {
                rollbackSucceeded = false;
                std::cerr << "Could not clean up directory: "
                          << it->string() << '\n';
            }
        }

        if (rollbackSucceeded) {
            if (!cleanupStaging()) {
                std::cerr << "Rollback succeeded, but staging cleanup failed: "
                          << stagingDir.string() << '\n';
            }

            std::cerr
                << "Restore failed; attempted working-tree changes "
                << "were rolled back.\n";
        } else {
            std::cerr
                << "Restore failed and rollback was incomplete. "
                << "Preserve the backups at: "
                << stagingDir.string() << '\n';
        }

        return false;
    }

    if (!cleanupStaging()) {
        std::cerr
            << "Restore succeeded, but staging cleanup failed. "
            << "Inspect: " << stagingDir.string() << '\n';
    }

    std::cout << "Restored commit: " << commitId << '\n';
    std::cout
        << "Branch pointer and commit history were not changed.\n";
    std::cout
        << "Files absent from the selected snapshot were preserved.\n";

    return true;
}
