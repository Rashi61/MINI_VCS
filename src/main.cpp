
#include <filesystem>
#include <iostream>
#include <string>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <system_error>
#include <cctype>

#include "FileManager.h"
#include "repository.h"
#include "HashManager.h"
#include "CommitManager.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "MiniVCS - Mini Version Control System\n";
        std::cout << "Commands:\n";
        std::cout << "  minivcs init [project-path]\n";
        std::cout << "  minivcs add <file>\n";
        std::cout << "  minivcs remove <file>\n";
        std::cout << "  minivcs tracked\n";
        std::cout << "  minivcs hash <file>\n";
        std::cout << "  minivcs commit <message>\n";
        std::cout << "  minivcs history\n";
        std::cout << "  minivcs status\n";
        std::cout << "  minivcs restore <commit-id>\n";
        return 0;
    }

    const std::string command = argv[1];

    if (command == "init") {
        const std::filesystem::path projectPath =
            (argc >= 3)
                ? std::filesystem::path(argv[2])
                : std::filesystem::current_path();

        Repository repository(projectPath);

        if (repository.isInitialized()) {
            std::cout << "Repository is already initialized.\n";
            return 0;
        }

        if (repository.init()) {
            std::cout << "MiniVCS repository initialized successfully.\n";
            std::cout << "Project: "
                      << repository.getRootPath().string() << '\n';
            std::cout << "Internal storage: "
                      << repository.getMetadataPath().string() << '\n';
            return 0;
        }

        std::cerr << "Failed to initialize repository.\n";
        return 1;
    }

    
    if (command == "hash") {
        if (argc != 3) {
            std::cerr << "Usage: minivcs hash <file>\n";
            return 1;
        }

        const std::string hash =
            HashManager::calculateHash(argv[2]);

        if (hash.empty()) {
            std::cerr << "Could not calculate file hash.\n";
            return 1;
        }

        std::cout << "SHA-256: " << hash << '\n';
        return 0;
    }

    Repository repository(std::filesystem::current_path());
    if (!repository.isInitialized()) {
        std::cerr
            << "Error: current directory is not an initialized "
            << "MiniVCS repository.\n";
        std::cerr
            << "Change into your project directory and try again.\n";
        return 1;
    }

    FileManager fileManager(repository.getRootPath());
    if (command == "commit") {
        if (argc < 3) {
            std::cerr << "Commit message cannot be empty.\n";
            return 1;
        }

        std::string message = argv[2];

        for (int i = 3; i < argc; ++i) {
            message += " ";
            message += argv[i];
        }

       const bool onlyWhitespace = std::all_of(
            message.begin(),
            message.end(),
            [](unsigned char ch) {
                return std::isspace(ch);
            }
        );

        if (onlyWhitespace) {
            std::cerr << "Commit message cannot be empty.\n";
            return 1;
        }

        CommitManager commitManager(repository.getRootPath());

        if (commitManager.createCommit(message)) {
            std::cout << "Commit created successfully!\n";
            std::cout << "Commit ID: "
                    << commitManager.getLatestCommitId() << '\n';
            return 0;
        }

        std::cerr << "Commit was not created. See the message above for details.\n";
        return 1;
    }

    if (command == "history") {
        CommitManager commitManager(repository.getRootPath());
        const auto commits = commitManager.getCommitHistory();

        if (commits.empty()) {
            std::cout << "No commits yet.\n";
            return 0;
        }

        for (auto it = commits.rbegin(); it != commits.rend(); ++it) {
            std::cout << "\nCommit: " << it->getId() << '\n';
            std::cout << "Message: " << it->getMessage() << '\n';
            std::cout << "Date: " << it->getTimestamp() << '\n';
        }

        return 0;
    }
    if (command == "restore") {
        if (argc != 3) {
            std::cerr << "Usage: minivcs restore <commit-id>\n";
            return 1;
        }

        CommitManager commitManager(repository.getRootPath());

        if (commitManager.restoreCommit(argv[2])) {
            return 0;
        }

        return 1;
    }


    if (command == "status") {
        const auto trackedFiles = fileManager.getTrackedFiles();

        CommitManager commitManager(repository.getRootPath());
        const auto history = commitManager.getCommitHistory();

        const auto root = repository.getRootPath();
        const auto previousHashes = history.empty()
            ? std::unordered_map<std::string, std::string>{}
            : history.back().getFileHashes();

        bool changesFound = false;

        for (const auto& relativePath : trackedFiles) {
            const auto filePath = root / relativePath;
            std::error_code ec;

            if (!std::filesystem::is_regular_file(filePath, ec) || ec) {
                std::cout << "Missing: " << relativePath << '\n';
                changesFound = true;
                continue;
            }

            const auto previous = previousHashes.find(relativePath);

            if (previous == previousHashes.end()) {
                std::cout << "Uncommitted: " << relativePath << '\n';
                changesFound = true;
                continue;
            }

            if (HashManager::hasChanged(filePath, previous->second)) {
                std::cout << "Modified: " << relativePath << '\n';
                changesFound = true;
            }
        }

        std::error_code ec;

        for (std::filesystem::recursive_directory_iterator it(
                root,
                std::filesystem::directory_options::skip_permission_denied,
                ec), end;
            it != end;
            it.increment(ec)) {

            if (ec) {
                ec.clear();
                continue;
            }

            const auto entry = *it;
            const auto relativePath =
                std::filesystem::relative(entry.path(), root, ec);

            if (ec) {
                ec.clear();
                continue;
            }

            const auto normalized = relativePath.generic_string();

            if (entry.is_directory(ec)) {
                if (normalized == ".minivcs" ||
                    normalized == ".git" ||
                    normalized == "build") {
                    it.disable_recursion_pending();
                }
                ec.clear();
                continue;
            }
            if (normalized == "minivcs.exe") {
                continue;
            }

            if (ec || !entry.is_regular_file(ec) || ec) {
                ec.clear();
                continue;
            }

            if (std::find(
                    trackedFiles.begin(),
                    trackedFiles.end(),
                    normalized) == trackedFiles.end()) {
                std::cout << "Untracked: " << normalized << '\n';
                changesFound = true;
            }
        }

        if (!changesFound) {
            std::cout << "Working tree clean.\n";
        }

        return 0;
    }

    if (command == "add") {
        if (argc != 3) {
            std::cerr << "Usage: minivcs add <file>\n";
            return 1;
        }

        const std::filesystem::path requestedPath(argv[2]);
        const std::string normalizedPath =
            requestedPath.lexically_normal().generic_string();

        const auto trackedFiles = fileManager.getTrackedFiles();

        if (std::find(
                trackedFiles.begin(),
                trackedFiles.end(),
                normalizedPath) != trackedFiles.end()) {
            std::cout << "File is already tracked: "
                    << normalizedPath << '\n';
            return 0;
        }

        if (fileManager.addFile(argv[2])) {
            std::cout << "File is now tracked: "
                    << normalizedPath << '\n';
            return 0;
        }

        std::cerr
            << "Could not track file. Check that it exists, is inside "
            << "the project, and is not inside .minivcs.\n";
        return 1;
    }
    if (command == "remove") {
        if (argc != 3) {
            std::cerr << "Usage: minivcs remove <file>\n";
            return 1;
        }
        if (fileManager.removeFile(argv[2])) {
            std::cout << "File is no longer tracked: "
                      << argv[2] << '\n';
            return 0;
        }
        std::cerr
            << "Could not remove file from tracking. "
            << "It may not be tracked.\n";
        return 1;
    }
    if (command == "tracked") {
        const auto files = fileManager.getTrackedFiles();
        if (files.empty()) {
            std::cout << "No files are currently tracked.\n";
            return 0;
        }
        std::cout << "Tracked files:\n";
        for (const auto& file : files) {
            std::cout << "  " << file << '\n';
        }
        return 0;
    }
    std::cerr << "Unknown command: " << command << '\n';
    return 1;
}
