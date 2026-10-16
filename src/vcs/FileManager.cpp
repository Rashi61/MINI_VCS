
#include "FileManager.h"

#include <algorithm>
#include <fstream>
#include <system_error>

namespace {

bool normalizeRelativePath(
    const std::string& input,
    std::string& normalized
) {
    if (input.empty()) {
        return false;
    }

    std::filesystem::path path(input);

    if (path.is_absolute() || path.has_root_name() ||
        path.has_root_directory()) {
        return false;
    }

    path = path.lexically_normal();

    if (path.empty() || path == ".") {
        return false;
    }

    for (const auto& part : path) {
        if (part == "..") {
            return false;
        }
    }

    auto first = path.begin();
    if (first != path.end() && *first == ".minivcs") {
        return false;
    }

    normalized = path.generic_string();
    return !normalized.empty();
}

} 

FileManager::FileManager(
    const std::filesystem::path& rootPath
) {
    repositoryRoot =
        std::filesystem::absolute(rootPath).lexically_normal();

    metadataPath = repositoryRoot / ".minivcs";
    trackedFilesPath = metadataPath / "tracked.txt";
}

bool FileManager::loadTrackedFiles(
    std::vector<std::string>& files
) const {
    files.clear();

    std::error_code ec;

    if (!std::filesystem::is_directory(metadataPath, ec) || ec) {
        return false;
    }

    if (!std::filesystem::exists(trackedFilesPath, ec)) {
        return !ec;
    }

    if (ec ||
        !std::filesystem::is_regular_file(trackedFilesPath, ec) ||
        ec) {
        return false;
    }

    std::ifstream input(trackedFilesPath);

    if (!input) {
        return false;
    }

    std::string line;

    while (std::getline(input, line)) {
        if (!line.empty()) {
            files.push_back(line);
        }
    }

    return input.eof() && !input.bad();
}


bool FileManager::saveTrackedFiles(
    const std::vector<std::string>& files
) const {
    std::error_code ec;

    if (!std::filesystem::is_directory(metadataPath, ec) || ec) {
        return false;
    }

    const auto temporaryPath = metadataPath / "tracked.tmp";
    const auto backupPath = metadataPath / "tracked.bak";

    {
        std::ofstream output(
            temporaryPath,
            std::ios::out | std::ios::trunc
        );

        if (!output) {
            return false;
        }

        for (const auto& file : files) {
            std::string normalized;

            if (!normalizeRelativePath(file, normalized)) {
                output.close();
                std::filesystem::remove(temporaryPath, ec);
                return false;
            }

            output << normalized << '\n';

            if (!output) {
                output.close();
                std::filesystem::remove(temporaryPath, ec);
                return false;
            }
        }

        output.close();

        if (!output) {
            std::filesystem::remove(temporaryPath, ec);
            return false;
        }
    }

    ec.clear();
    const bool hadOriginal =
        std::filesystem::exists(trackedFilesPath, ec);

    if (ec) {
        std::filesystem::remove(temporaryPath, ec);
        return false;
    }

    if (hadOriginal) {
        std::filesystem::remove(backupPath, ec);

        if (ec) {
            std::filesystem::remove(temporaryPath, ec);
            return false;
        }

        std::filesystem::rename(
            trackedFilesPath, backupPath, ec
        );

        if (ec) {
            std::filesystem::remove(temporaryPath, ec);
            return false;
        }
    }

    ec.clear();
    std::filesystem::rename(
        temporaryPath, trackedFilesPath, ec
    );

    if (ec) {
        if (hadOriginal) {
            std::error_code restoreError;
            std::filesystem::rename(
                backupPath, trackedFilesPath, restoreError
            );
        }

        std::error_code cleanupError;
        std::filesystem::remove(temporaryPath, cleanupError);
        return false;
    }

    if (hadOriginal) {
        std::filesystem::remove(backupPath, ec);
    }

    return true;
}

bool FileManager::addFile(const std::string& relativePath) {
    std::string normalized;

    if (!normalizeRelativePath(relativePath, normalized)) {
        return false;
    }

    const auto fullPath = repositoryRoot / normalized;
    std::error_code ec;

    if (!std::filesystem::is_regular_file(fullPath, ec) || ec) {
        return false;
    }

    std::vector<std::string> files;

    if (!loadTrackedFiles(files)) {
        return false;
    }

    if (std::find(files.begin(), files.end(), normalized)
        != files.end()) {
        return true;
    }

    files.push_back(normalized);

    return saveTrackedFiles(files);
}

bool FileManager::removeFile(const std::string& relativePath) {
    std::string normalized;

    if (!normalizeRelativePath(relativePath, normalized)) {
        return false;
    }

    std::vector<std::string> files;

    if (!loadTrackedFiles(files)) {
        return false;
    }

    const auto oldSize = files.size();

    files.erase(
        std::remove(files.begin(), files.end(), normalized),
        files.end()
    );

    if (files.size() == oldSize) {
        return false; 
    }

    return saveTrackedFiles(files);
}

std::vector<std::string> FileManager::getTrackedFiles() const {
    std::vector<std::string> files;

    if (!loadTrackedFiles(files)) {
        return {};
    }

    return files;
}
