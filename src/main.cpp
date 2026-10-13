
#include <filesystem>
#include <iostream>
#include <string>

#include "FileManager.h"
#include "Repository.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "MiniVCS - Mini Version Control System\n";
        std::cout << "Commands:\n";
        std::cout << "  minivcs init [project-path]\n";
        std::cout << "  minivcs add <file>\n";
        std::cout << "  minivcs remove <file>\n";
        std::cout << "  minivcs tracked\n";
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

    // Run add, remove, and tracked from inside the initialized project.
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

    if (command == "add") {
        if (argc != 3) {
            std::cerr << "Usage: minivcs add <file>\n";
            return 1;
        }

        if (fileManager.addFile(argv[2])) {
            std::cout << "File is now tracked: " << argv[2] << '\n';
            return 0;
        }

        std::cerr
            << "Could not track file. Check that it exists, is inside "
            << "the project, and is not inside .minigit.\n";
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
