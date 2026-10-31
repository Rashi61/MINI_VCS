#include <filesystem>
#include <iostream>
#include <string>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <system_error>
#include <cctype>
#include <fstream>
#include <vector>

#include "FileManager.h"
#include "IgnoreMatcher.h"
#include "repository.h"
#include "HashManager.h"
#include "CommitManager.h"
#include "DiffManager.h"
#include "IntegrityManager.h"
#include "branchmanager.h"
#include "RetentionManager.h"
#include "member3.h"
namespace {

struct IgnoreRules {
    std::vector<std::string> builtIn;
    std::vector<std::string> userRules;
};

IgnoreRules loadIgnoreRules(const std::filesystem::path& root) {
    IgnoreRules rules;
    rules.builtIn = {".minivcs", ".git", ".vs", ".idea", ".vscode", "out",
                     "build", "minivcs", "minivcs.exe"};

    std::ifstream input(root / ".minivcsignore");
    std::string line;

    while (std::getline(input, line)) {
        while (!line.empty() &&
               std::isspace(static_cast<unsigned char>(line.back()))) {
            line.pop_back();
        }

        std::size_t start = 0;
        while (start < line.size() &&
               std::isspace(static_cast<unsigned char>(line[start]))) {
            ++start;
        }
        line = line.substr(start);

        if (line.empty() || line[0] == '#') {
            continue;
        }

        while (!line.empty() && line.back() == '/') {
            line.pop_back();
        }

        if (!line.empty()) {
            rules.userRules.push_back(line);
        }
    }

    return rules;
}

bool matchesPath(const std::string& path, const std::string& rule) {
    return path == rule ||
           (path.size() > rule.size() &&
            path.compare(0, rule.size(), rule) == 0 &&
            path[rule.size()] == '/');
}

bool matchesComponent(const std::string& path, const std::string& rule) {
    std::size_t begin = 0;

    while (begin <= path.size()) {
        std::size_t end = path.find('/', begin);
        if (end == std::string::npos) {
            end = path.size();
        }
        if (path.compare(begin, end - begin, rule) == 0) {
            return true;
        }
        begin = end + 1;
    }

    return false;
}

bool isIgnored(const std::string& path, const IgnoreRules& rules) {
    const std::string first = path.substr(0, path.find('/'));
    if (first.rfind("cmake-build-", 0) == 0 || first.rfind("build-", 0) == 0) {
        return true;
    }

    for (const auto& rule : rules.builtIn) {
        if (matchesPath(path, rule)) {
            return true;
        }
    }

    for (const auto& rule : rules.userRules) {
        if (matchesPath(path, rule) ||
            (rule.find('/') == std::string::npos &&
             matchesComponent(path, rule))) {
            return true;
        }
    }

    return false;
}
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "MiniVCS - Mini Version Control System\n";
        std::cout << "Commands:\n";
        std::cout << "  minivcs auth\n";
        std::cout << "  minivcs init [project-path]\n";
        std::cout << "  minivcs add <file>\n";
        std::cout << "  minivcs remove <file>\n";
        std::cout << "  minivcs tracked\n";
        std::cout << "  minivcs hash <file>\n";
        std::cout << "  minivcs commit <message>\n";
        std::cout << "  minivcs history\n";
        std::cout << "  minivcs status\n";
        std::cout << "  minivcs restore <commit-id|prefix> [--force]\n";
        std::cout << "  minivcs show <commit-id|prefix>\n";
        std::cout << "  minivcs diff [<commit>] [<commit>]\n";
        std::cout << "  minivcs verify\n";
        std::cout << "  minivcs retention [status|set <days>|cleanup "
                     "[--dry-run]|log]\n";
        std::cout<< "  minivcs branch create <name>\n";
        std::cout<< "  minivcs branches\n";
        std::cout<< "  minivcs switch <name> [--force]\n";
        std::cout<< "  minivcs delete-branch <name>\n";
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
    // ===== Member 3: Authentication and Database Management =====

if (command == "auth") {
        const auto discoveredRoot =
        Repository::discoverRoot(
            std::filesystem::current_path()
        );

    if (!discoveredRoot.has_value()) {
        std::cerr
            << "Error: current directory is not inside an "
            << "initialized MiniVCS repository.\n";
        return 1;
    }

    const std::filesystem::path databasePath =
        *discoveredRoot /
        ".minivcs" /
        "minivcs_users.db";

    Member3 member3(databasePath.string());

    if (!member3.initializeDatabase()) {
        std::cerr << "Database initialization failed.\n";
        return 1;
    }

    int choice;

    do {
        std::cout << "\n===== MiniVCS Member 3 Menu =====\n";
        std::cout << "1. Register User\n";
        std::cout << "2. Login\n";
        std::cout << "3. Logout\n";
        std::cout << "4. Check Login Status\n";
        std::cout << "5. Create Repository\n";
        std::cout << "6. View Repositories\n";
        std::cout << "7. Add Collaborator\n";
        std::cout << "8. View Collaborators\n";
        std::cout << "9. Update Collaborator Role\n";
        std::cout << "10. Remove Collaborator\n";
        std::cout << "11. Exit\n";
        std::cout << "Enter your choice: ";

        if (!(std::cin >> choice)) {
            std::cerr << "Invalid input.\n";
            return 1;
        }

        std::string username, email, password, repoName, visibility, role;
        int repoId;

        switch (choice) {

        case 1:
            std::cout << "Username: ";
            std::cin >> username;
            std::cout << "Email: ";
            std::cin >> email;
            std::cout << "Password: ";
            std::cin >> password;
            member3.registerUser(username, email, password);
            break;

        case 2:
            std::cout << "Username: ";
            std::cin >> username;
            std::cout << "Password: ";
            std::cin >> password;
            member3.login(username, password);
            break;

        case 3:
            member3.logout();
            break;

        case 4:
            std::cout << (member3.isLoggedIn()
                ? "User is logged in.\n"
                : "No user is logged in.\n");
            break;

        case 5:
            std::cout << "Repository name: ";
            std::cin >> repoName;
            std::cout << "Visibility (public/private): ";
            std::cin >> visibility;
            member3.createRepository(repoName, visibility);
            break;

        case 6:
            member3.viewRepositories();
            break;

        case 7:
            std::cout << "Repository ID: ";
            std::cin >> repoId;
            std::cout << "Collaborator username: ";
            std::cin >> username;
            std::cout << "Role: ";
            std::cin >> role;
            member3.addCollaborator(repoId, username, role);
            break;

        case 8:
            std::cout << "Repository ID: ";
            std::cin >> repoId;
            member3.viewCollaborators(repoId);
            break;

        case 9:
            std::cout << "Repository ID: ";
            std::cin >> repoId;
            std::cout << "Collaborator username: ";
            std::cin >> username;
            std::cout << "New role: ";
            std::cin >> role;
            member3.updateCollaboratorRole(repoId, username, role);
            break;

        case 10:
            std::cout << "Repository ID: ";
            std::cin >> repoId;
            std::cout << "Collaborator username: ";
            std::cin >> username;
            member3.removeCollaborator(repoId, username);
            break;

        case 11:
            std::cout << "Exiting Member 3 menu.\n";
            break;

        default:
            std::cout << "Invalid choice.\n";
        }

    } while (choice != 11);

    return 0;
}

// ===== Existing Repository Operations =====

   const auto discoveredRoot =
    Repository::discoverRoot(std::filesystem::current_path());

    if (!discoveredRoot.has_value()) {
        std::cerr
            << "Error: current directory is not inside an initialized "
            << "MiniVCS repository.\n";
        return 1;
    }

    Repository repository(*discoveredRoot);

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
        const HistoryResult history =
            commitManager.readCommitHistory();

        if (!history.valid) {
            std::cerr
                << "Error: corrupt repository history: "
                << history.error << '\n';

            return 1;
        }

        if (history.commits.empty()) {
            std::cout << "No commits yet.\n";
            return 0;
        }

        for (auto it = history.commits.rbegin();
             it != history.commits.rend();
             ++it) {
            std::cout << "\nCommit: "
                      << it->getId() << '\n';

            std::cout << "Message: "
                      << it->getMessage() << '\n';

            std::cout << "Date: "
                      << it->getTimestamp() << '\n';

            std::cout << "Parent: "
                      << (it->getParentId().empty()
                              ? std::string("none")
                              : it->getParentId())
                      << '\n';
        }

        return 0;
    }

    if (command == "restore") {
        bool force = false;
        std::string requestedId;

        for (int i = 2; i < argc; ++i) {
            const std::string arg = argv[i];

            if (arg == "--force") {
                force = true;
            } else if (requestedId.empty()) {
                requestedId = arg;
            } else {
                requestedId.clear();
                break;
            }
        }

        if (requestedId.empty()) {
            std::cerr << "Usage: minivcs restore <commit-id|prefix> "
                         "[--force]\n";
            return 1;
        }

        CommitManager commitManager(repository.getRootPath());

        return commitManager.restoreCommit(requestedId, force) ? 0 : 1;
    }

    if (command == "show") {
        if (argc != 3) {
            std::cerr << "Usage: minivcs show <commit-id|prefix>\n";
            return 1;
        }

        DiffManager diffManager(repository.getRootPath());
        return diffManager.showCommit(argv[2], std::cout) ? 0 : 1;
    }

    if (command == "diff") {
        if (argc > 4) {
            std::cerr << "Usage: minivcs diff [<commit>] [<commit>]\n";
            return 1;
        }

        DiffManager diffManager(repository.getRootPath());

        if (argc == 4) {
            return diffManager.diffCommits(argv[2], argv[3], std::cout)
                       ? 0 : 1;
        }

        return diffManager.diffWorkingTree(argc == 3 ? argv[2] : "",
                                           std::cout)
                   ? 0 : 1;
    }

    if (command == "verify") {
        if (argc != 2) {
            std::cerr << "Usage: minivcs verify\n";
            return 1;
        }

        IntegrityManager integrityManager(repository.getRootPath());
        const IntegrityReport report =
            integrityManager.verifyRepository();

        std::cout << "Repository integrity check\n\n";
        std::cout << "Checked branches: "
                  << report.checkedBranches << '\n';
        std::cout << "Checked commits: "
                  << report.checkedCommits << '\n';
        std::cout << "Checked snapshots: "
                  << report.checkedSnapshots << '\n';

        int warnings = 0;
        int errors = 0;

        for (const auto& issue : report.issues) {
            if (issue.warning) {
                ++warnings;
                std::cout << "Warning: ";
            } else {
                ++errors;
                std::cout << "Error: ";
            }

            std::cout << '[' << issue.category << "] "
                      << issue.message;

            if (!issue.path.empty()) {
                std::cout << " (" << issue.path << ')';
            }

            std::cout << '\n';
        }

        std::cout << "\nErrors: " << errors << '\n';
        std::cout << "Warnings: " << warnings << '\n';

        if (!report.valid) {
            std::cout << "Repository is corrupt.\n";
            return 1;
        }

        std::cout << "Repository is valid.\n";
        return 0;
    }

    if (command == "retention") {
        const auto metadata = repository.getMetadataPath();
        CommitManager commitManager(repository.getRootPath());

        RetentionManager retention(
            (metadata / "objects").string(),
            (metadata / "retention.cfg").string());
        retention.setProtected(commitManager.getBranchTips());

        const std::string action = (argc >= 3) ? argv[2] : "status";

        if (action == "set") {
            if (argc != 4 ||
                std::string(argv[3]).find_first_not_of("0123456789") !=
                    std::string::npos ||
                std::string(argv[3]).size() > 6) {
                std::cerr << "Usage: minivcs retention set <days>\n";
                return 1;
            }

            retention.setRetentionDays(std::stoi(argv[3]));
            std::cout << "Retention set to "
                      << retention.getRetentionDays() << " days.\n";
            return 0;
        }

        if (action == "status") {
            std::cout << "Retention: " << retention.getRetentionDays()
                      << " days\n";

            for (const auto& info : retention.listSnapshots()) {
                if (!info.isCommit) {
                    continue;
                }

                std::string state = "keep";
                if (info.pruned)            state = "pruned (history kept)";
                else if (info.latest)       state = "keep (newest)";
                else if (info.protectedTip) state = "keep (branch tip)";
                else if (info.expired)      state = "EXPIRED";

                std::cout << "  " << info.name.substr(0, 8) << "  "
                          << info.date << "  " << info.ageDays << "d  "
                          << state << '\n';
            }

            return 0;
        }

        if (action == "cleanup") {
            const bool dryRun = (argc == 4 && std::string(argv[3]) == "--dry-run");

            if (argc > 4 || (argc == 4 && !dryRun)) {
                std::cerr << "Usage: minivcs retention cleanup [--dry-run]\n";
                return 1;
            }

            if (dryRun) {
                const auto expired = retention.findExpired();
                std::cout << expired.size() << " commit snapshot(s) would be "
                             "removed.\n";
                for (const auto& info : expired) {
                    std::cout << "  " << info.name.substr(0, 8) << "  "
                              << info.date << '\n';
                }
                return 0;
            }

            const int removed = retention.cleanup();
            std::cout << removed << " commit snapshot(s) removed. "
                         "Commit history was kept.\n";
            return 0;
        }

        if (action == "log") {
            const auto lines = retention.expiredLog();
            if (lines.empty()) {
                std::cout << "No snapshots have been removed yet.\n";
            }
            for (const auto& line : lines) {
                std::cout << line << '\n';
            }
            return 0;
        }

        std::cerr << "Usage: minivcs retention "
                     "[status|set <days>|cleanup [--dry-run]|log]\n";
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

        const IgnoreMatcher ignoreMatcher(root);
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

            if (ignoreMatcher.isIgnored(normalized)) {
                if (entry.is_directory(ec)) {
                    it.disable_recursion_pending();
                }
                ec.clear();
                continue;
            }

            if (entry.is_directory(ec)) {
                ec.clear();
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
        const IgnoreMatcher ignoreMatcher(
            repository.getRootPath()
        );

        if (ignoreMatcher.isIgnored(normalizedPath)) {
            std::cerr
                << "Cannot add ignored path: "
                << normalizedPath << '\n';
            return 1;
        }

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

    // ===== Member 2: branch / switch / merge commands =====
        if (command == "branch") {
        if (argc != 4 ||
            std::string(argv[2]) != "create") {
            std::cerr
                << "Usage: minivcs branch create <name>\n";
            return 1;
        }

        BranchManager branchManager(
            repository.getRootPath()
        );

        if (!branchManager.createBranch(argv[3])) {
            std::cerr
                << "Could not create branch.\n";
            return 1;
        }

        std::cout
            << "Branch created: "
            << argv[3] << '\n';

        return 0;
    }

    if (command == "branches") {
        if (argc != 2) {
            std::cerr
                << "Usage: minivcs branches\n";
            return 1;
        }

        BranchManager branchManager(
            repository.getRootPath()
        );

        const auto branches =
            branchManager.listBranches();

        for (const auto& branch : branches) {
            if (branch ==
                branchManager.getCurrentBranch()) {
                std::cout << "* ";
            } else {
                std::cout << "  ";
            }

            std::cout << branch << '\n';
        }

        return 0;
    }

    if (command == "switch") {
        if (argc < 3 || argc > 4) {
            std::cerr
                << "Usage: minivcs switch <name> [--force]\n";
            return 1;
        }

        const bool force =
            argc == 4 &&
            std::string(argv[3]) == "--force";

        if (argc == 4 && !force) {
            std::cerr
                << "Unknown option: "
                << argv[3] << '\n';
            return 1;
        }

        BranchManager branchManager(
            repository.getRootPath()
        );

        if (!branchManager.switchBranch(
                argv[2],
                force
            )) {
            std::cerr
                << "Could not switch branch. "
                << "The branch may not exist or "
                << "the working tree contains "
                << "conflicting changes.\n";
            return 1;
        }

        std::cout
            << "Switched to branch: "
            << argv[2] << '\n';

        return 0;
    }

    if (command == "delete-branch") {
        if (argc != 3) {
            std::cerr
                << "Usage: minivcs delete-branch <name>\n";
            return 1;
        }

        BranchManager branchManager(
            repository.getRootPath()
        );

        if (!branchManager.deleteBranch(argv[2])) {
            std::cerr
                << "Could not delete branch.\n";
            return 1;
        }

        std::cout
            << "Branch deleted: "
            << argv[2] << '\n';

        return 0;
    }

    // ===== Member 3: user / login / permission commands =====

    // ===== Member 4: clone / push / pull commands =====

    
    std::cerr << "Unknown command: " << command << '\n';
    return 1;
}