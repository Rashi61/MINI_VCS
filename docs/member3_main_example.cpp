#include <vector>
#include <iterator>
#include <utility>
#include<string>
#include<fstream>
#include <iostream>
#include <filesystem>
#include "sqlite3.h"

namespace fs = std::filesystem;

void initRepository() {
    if (fs::exists(".minivcs")) {
        std::cout << "Repository already initialized!\n";
        return;
    }

    fs::create_directory(".minivcs");

    sqlite3* db = nullptr;

    if (sqlite3_open(".minivcs/minivcs.db", &db) != SQLITE_OK) {
        std::cout << "Database connection failed!\n";
        if (db != nullptr) {
            sqlite3_close(db);
        }
        return;
    }

    const char* sql =
        "CREATE TABLE IF NOT EXISTS commits ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "message TEXT,"
        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";

    char* errorMessage = nullptr;

    int result = sqlite3_exec(
        db, sql, nullptr, nullptr, &errorMessage
    );

    if (result != SQLITE_OK) {
        std::cout << "Error: " << errorMessage << "\n";
        sqlite3_free(errorMessage);
    } else {
        std::cout << "MiniVCS repository initialized successfully!\n";
    }

    sqlite3_close(db);
}
void addFile(const std::string& filename) {

    if (!fs::exists(".minivcs/minivcs.db")) {
        std::cout << "Initialize repository first!\n";
        return;
    }

    if (!fs::exists(filename) || !fs::is_regular_file(filename)) {
        std::cout << "File does not exist!\n";
        return;
    }

    sqlite3* db = nullptr;

    if (sqlite3_open(".minivcs/minivcs.db", &db) != SQLITE_OK) {
        std::cout << "Database connection failed!\n";
        if (db != nullptr) sqlite3_close(db);
        return;
    }

    const char* sql =
        "CREATE TABLE IF NOT EXISTS staging ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "filename TEXT UNIQUE"
        ");";

    if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) != SQLITE_OK) {
        std::cout << "Could not create staging table!\n";
        sqlite3_close(db);
        return;
    }

    sqlite3_stmt* stmt = nullptr;

    const char* insertSQL =
        "INSERT OR IGNORE INTO staging (filename) VALUES (?);";

    if (sqlite3_prepare_v2(db, insertSQL, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cout << "Could not prepare staging query!\n";
        sqlite3_close(db);
        return;
    }

    sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_DONE) {
        std::cout << "File added to staging: " << filename << "\n";
    } else {
        std::cout << "Failed to stage file!\n";
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}
void commitChanges() {
    if (!fs::exists(".minivcs/minivcs.db")) {
        std::cout << "Initialize repository first!\n";
        return;
    }

    sqlite3* db = nullptr;

    if (sqlite3_open(".minivcs/minivcs.db", &db) != SQLITE_OK) {
        std::cout << "Database connection failed!\n";
        if (db) sqlite3_close(db);
        return;
    }

    const char* createSQL =
        "CREATE TABLE IF NOT EXISTS file_versions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "commit_id INTEGER,"
        "filename TEXT,"
        "content BLOB"
        ");";

    if (sqlite3_exec(db, createSQL, nullptr, nullptr, nullptr) != SQLITE_OK) {
        std::cout << "Failed to create file_versions table!\n";
        sqlite3_close(db);
        return;
    }

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(
            db,
            "SELECT filename FROM staging;",
            -1, &stmt, nullptr) != SQLITE_OK) {
        std::cout << "Failed to read staged files!\n";
        sqlite3_close(db);
        return;
    }

    std::vector<std::pair<std::string, std::string>> files;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string filename =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));

        std::ifstream file(filename, std::ios::binary);

        if (!file) {
            std::cout << "Cannot read file: " << filename << "\n";
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return;
        }

        std::string content(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()
        );

        files.push_back({filename, content});
    }

    sqlite3_finalize(stmt);

    if (files.empty()) {
        std::cout << "No files staged for commit!\n";
        sqlite3_close(db);
        return;
    }

    std::string message;
    std::cout << "Enter commit message: ";
    std::getline(std::cin >> std::ws, message);

    if (sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        std::cout << "Could not start transaction!\n";
        sqlite3_close(db);
        return;
    }

    bool success = true;
    sqlite3_stmt* insertStmt = nullptr;

    if (sqlite3_prepare_v2(
            db,
            "INSERT INTO commits (message) VALUES (?);",
            -1, &insertStmt, nullptr) != SQLITE_OK) {
        success = false;
    } else {
        sqlite3_bind_text(insertStmt, 1, message.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(insertStmt) != SQLITE_DONE)
            success = false;
    }

    sqlite3_finalize(insertStmt);

    sqlite3_int64 commitId = sqlite3_last_insert_rowid(db);

    if (success) {
        if (sqlite3_prepare_v2(
                db,
                "INSERT INTO file_versions (commit_id, filename, content) "
                "VALUES (?, ?, ?);",
                -1, &insertStmt, nullptr) != SQLITE_OK) {
            success = false;
        } else {
            for (const auto& entry : files) {
                sqlite3_reset(insertStmt);
                sqlite3_clear_bindings(insertStmt);

                sqlite3_bind_int64(insertStmt, 1, commitId);
                sqlite3_bind_text(insertStmt, 2, entry.first.c_str(),
                                  -1, SQLITE_TRANSIENT);
                sqlite3_bind_blob(insertStmt, 3, entry.second.data(),
                                  static_cast<int>(entry.second.size()),
                                  SQLITE_TRANSIENT);

                if (sqlite3_step(insertStmt) != SQLITE_DONE) {
                    success = false;
                    break;
                }
            }
        }
    }

    sqlite3_finalize(insertStmt);

    if (success &&
        sqlite3_exec(db, "DELETE FROM staging;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        success = false;
    }

    if (success &&
        sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr) == SQLITE_OK) {
        std::cout << "Commit created successfully! Commit ID: "
                  << commitId << "\n";
    } else {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        std::cout << "Commit failed!\n";
    }

    sqlite3_close(db);
}
int main() {
    int choice;
    std::string filename;

    do {
        std::cout << "\n===== MiniVCS Menu =====\n";
        std::cout << "1. Initialize Repository\n";
        std::cout << "2. Add File\n";
        std::cout << "3. Commit Changes\n";
        std::cout << "4. Exit\n";
        std::cout << "Enter your choice: ";

        if (!(std::cin >> choice)) {
            std::cout << "Invalid input!\n";
            break;
        }

        switch (choice) {
            case 1:
                initRepository();
                break;

            case 2:
                std::cout << "Enter filename: ";
                std::cin >> filename;
                addFile(filename);
                break;

            case 3:
                commitChanges();
                break;

            case 4:
                std::cout << "Exiting MiniVCS...\n";
                break;

            default:
                std::cout << "Invalid choice!\n";
        }

    } while (choice != 4);

    return 0;
}