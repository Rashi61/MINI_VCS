#include "Member3.h"
#include <iostream>
#include <windows.h>
#include <bcrypt.h>
#include <vector>

// Constructor: Connect to SQLite database
Member3::Member3(const std::string& databasePath) {
    db = nullptr;

    int result = sqlite3_open(databasePath.c_str(), &db);

    if (result == SQLITE_OK) {
        std::cout << "Database connected successfully!\n";
    }
    else {
        std::cout << "Database connection failed!\n";

        if (db != nullptr) {
            sqlite3_close(db);
            db = nullptr;
        }
    }
}

// Destructor: Close database
Member3::~Member3() {
    if (db != nullptr) {
        sqlite3_close(db);
    }
}

// Create database tables
bool Member3::initializeDatabase() {

    if (db == nullptr) {
        std::cout << "Database is not connected!\n";
        return false;
    }

    const char* sql = R"SQL(

        PRAGMA foreign_keys = ON;

        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL UNIQUE,
            email TEXT NOT NULL UNIQUE,
            password_hash TEXT NOT NULL,
            password_salt TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS repositories (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            owner_id INTEGER NOT NULL,
            visibility TEXT NOT NULL DEFAULT 'private'
                CHECK(visibility IN ('public', 'private')),
            FOREIGN KEY(owner_id) REFERENCES users(id)
        );

        CREATE TABLE IF NOT EXISTS collaborators (
            repository_id INTEGER NOT NULL,
            user_id INTEGER NOT NULL,
            role TEXT NOT NULL
                CHECK(role IN ('read', 'write', 'admin')),
            PRIMARY KEY(repository_id, user_id),
            FOREIGN KEY(repository_id) REFERENCES repositories(id),
            FOREIGN KEY(user_id) REFERENCES users(id)
        );

    )SQL";

    char* errorMessage = nullptr;

    int result = sqlite3_exec(
        db,
        sql,
        nullptr,
        nullptr,
        &errorMessage
    );

    if (result != SQLITE_OK) {
        std::cout << "Error creating tables: "
                  << (errorMessage ? errorMessage : "Unknown error")
                  << "\n";

        sqlite3_free(errorMessage);
        return false;
    }

    std::cout << "Database tables created successfully!\n";

    return true;
}
// Convert bytes into hexadecimal text
static std::string bytesToHex(
    const std::vector<unsigned char>& bytes
) {
    const char* digits = "0123456789abcdef";
    std::string result;

    for (unsigned char byte : bytes) {
        result += digits[byte >> 4];
        result += digits[byte & 15];
    }

    return result;
}
// Convert hexadecimal text back into bytes
static std::vector<unsigned char> hexToBytes(
    const std::string& text
) {
    std::vector<unsigned char> result;

    if (text.size() % 2 != 0)
        return result;

    auto hexValue = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };

    for (size_t i = 0; i < text.size(); i += 2) {
        int first = hexValue(text[i]);
        int second = hexValue(text[i + 1]);

        if (first < 0 || second < 0)
            return {};

        result.push_back(
            static_cast<unsigned char>((first << 4) | second)
        );
    }

    return result;
}
// Register a new user
bool Member3::registerUser(
    const std::string& username,
    const std::string& email,
    const std::string& password
) {
    if (db == nullptr) {
        std::cout << "Database is not connected!\n";
        return false;
    }

    if (username.empty() || email.empty() || password.empty()) {
        std::cout << "All fields are required!\n";
        return false;
    }

    // Generate a random 16-byte salt
    std::vector<unsigned char> salt(16);

    NTSTATUS status = BCryptGenRandom(
        nullptr,
        salt.data(),
        static_cast<ULONG>(salt.size()),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG
    );

    if (status < 0) {
        std::cout << "Salt generation failed!\n";
        return false;
    }

    // Open SHA-256 in HMAC mode for PBKDF2
    BCRYPT_ALG_HANDLE algorithm = nullptr;

    status = BCryptOpenAlgorithmProvider(
        &algorithm,
        BCRYPT_SHA256_ALGORITHM,
        nullptr,
        BCRYPT_ALG_HANDLE_HMAC_FLAG
    );

    if (status < 0) {
        std::cout << "Password hashing initialization failed!\n";
        return false;
    }

    std::vector<unsigned char> hash(32);

    // PBKDF2-HMAC-SHA256 with 310,000 iterations
    status = BCryptDeriveKeyPBKDF2(
        algorithm,
        reinterpret_cast<PUCHAR>(
            const_cast<char*>(password.data())
        ),
        static_cast<ULONG>(password.size()),
        salt.data(),
        static_cast<ULONG>(salt.size()),
        310000,
        hash.data(),
        static_cast<ULONG>(hash.size()),
        0
    );

    BCryptCloseAlgorithmProvider(algorithm, 0);

    if (status < 0) {
        std::cout << "Password hashing failed!\n";
        return false;
    }

    std::string hashText = bytesToHex(hash);
    std::string saltText = bytesToHex(salt);

    // Insert user using a prepared SQL statement
    const char* sql =
        "INSERT INTO users "
        "(username, email, password_hash, password_salt) "
        "VALUES (?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(
        db, sql, -1, &stmt, nullptr
    ) != SQLITE_OK) {
        std::cout << "Could not prepare registration query!\n";
        return false;
    }

    sqlite3_bind_text(
        stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT
    );

    sqlite3_bind_text(
        stmt, 2, email.c_str(), -1, SQLITE_TRANSIENT
    );

    sqlite3_bind_text(
        stmt, 3, hashText.c_str(), -1, SQLITE_TRANSIENT
    );

    sqlite3_bind_text(
        stmt, 4, saltText.c_str(), -1, SQLITE_TRANSIENT
    );

    bool success = sqlite3_step(stmt) == SQLITE_DONE;

    sqlite3_finalize(stmt);

    if (success) {
        std::cout << "Registration successful!\n";
    } else {
        std::cout
            << "Registration failed. Username or email may already exist.\n";
    }

    return success;
}

