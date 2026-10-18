
#include "repository.h"

#include <fstream>
#include <system_error>

Repository::Repository(const std::filesystem::path& path)
    : rootPath(std::filesystem::absolute(path).lexically_normal()) {
}

std::filesystem::path Repository::getRootPath() const {
    return rootPath;
}

std::filesystem::path Repository::getMetadataPath() const {
    return rootPath / ".minivcs";
}

bool Repository::isInitialized() const {
    std::error_code ec;
    const auto metadata = getMetadataPath();
    if (!std::filesystem::is_directory(rootPath, ec) || ec) {
        return false;
    }
    ec.clear();
    if (!std::filesystem::is_directory(metadata, ec) || ec) {
        return false;
    }
    ec.clear();
    if (!std::filesystem::is_directory(
            metadata / "objects", ec) || ec) {
        return false;
    }
    ec.clear();
    if (!std::filesystem::is_directory(
            metadata / "branches", ec) || ec) {
        return false;
    }
    ec.clear();
    if (!std::filesystem::is_regular_file(
            metadata / "config", ec) || ec) {
        return false;
    }
    ec.clear();
    if (!std::filesystem::is_regular_file(
            metadata / "HEAD", ec) || ec) {
        return false;
    }
    std::ifstream config(metadata / "config");
    std::string line;
    while (std::getline(config, line)) {
        if (line == "repository_format=1") {
            return true;
        }
    }
    return false;
}

bool Repository::init() const {
    std::error_code ec;

    if (!std::filesystem::is_directory(rootPath, ec) || ec) {
        return false;
    }

    const auto metadata = getMetadataPath();

    if (std::filesystem::exists(metadata, ec) || ec) {
        return false;
    }

    std::filesystem::create_directories(
        metadata / "objects", ec);
        
    if (ec) {
        return false;
    }

    std::filesystem::create_directories(
        metadata / "branches", ec);

    if (ec) {
        return false;
    }

    std::ofstream config(metadata / "config");
    if (!config) {
        return false;
    }
    config << "repository_format=1\n";
    config.close();
    if (!config) {
        return false;
    }
    std::ofstream head(metadata / "HEAD");
    if (!head) {
        return false;
    }
    head << "main\n";
    head.close();
    if (!head) {
        return false;
    }
    std::ofstream mainBranch(metadata / "branches" / "main");
    if (!mainBranch) {
        return false;
    }

    mainBranch << '\n';
    mainBranch.close();

    if (!mainBranch) {
        return false;
    }
    return isInitialized();
}
