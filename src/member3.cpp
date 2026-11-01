#include "member3.h"
#include "PasswordHasher.h"

#include <iostream>
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

        CREATE TABLE IF NOT EXISTS repository_metadata (
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
            FOREIGN KEY(repository_id) REFERENCES repository_metadata(id),
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
    PasswordHash passwordHash;

if (!PasswordHasher::createHash(
        password,
        passwordHash
    )) {
    std::cout << "Password hashing failed!\n";
    return false;
}

const std::string& hashText =
    passwordHash.hash;

const std::string& saltText =
    passwordHash.salt;
    
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
// Display collaborators of a repository
void Member3::viewCollaborators(int repoId)
{
    if (!isLoggedIn())
    {
        std::cout << "Please login first!\n";
        return;
    }

    sqlite3_stmt* stmt = nullptr;

    // Verify that the user owns the repository
    // or is an existing collaborator.
    const char* accessSql =
        "SELECT name FROM repository_metadata "
        "WHERE id = ? AND (owner_id = ? OR EXISTS ("
        "SELECT 1 FROM collaborators "
        "WHERE repository_id = ? AND user_id = ?))";

    if (sqlite3_prepare_v2(db, accessSql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        std::cout << "Error checking repository access: "
                  << sqlite3_errmsg(db) << "\n";
        return;
    }

    sqlite3_bind_int(stmt, 1, repoId);
    sqlite3_bind_int(stmt, 2, currentUserId);
    sqlite3_bind_int(stmt, 3, repoId);
    sqlite3_bind_int(stmt, 4, currentUserId);

    std::string repoName;

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char* name = sqlite3_column_text(stmt, 0);

        if (name != nullptr)
            repoName = reinterpret_cast<const char*>(name);
    }

    sqlite3_finalize(stmt);

    if (repoName.empty())
    {
        std::cout << "Repository not found or access denied!\n";
        return;
    }

    // Retrieve collaborators and their roles
    const char* sql =
        "SELECT users.username, collaborators.role "
        "FROM collaborators "
        "JOIN users ON collaborators.user_id = users.id "
        "WHERE collaborators.repository_id = ? "
        "ORDER BY users.username;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        std::cout << "Error retrieving collaborators: "
                  << sqlite3_errmsg(db) << "\n";
        return;
    }

    sqlite3_bind_int(stmt, 1, repoId);

    std::cout << "\n===== REPOSITORY COLLABORATORS =====\n";
    std::cout << "Repository: " << repoName << "\n\n";

    int count = 0;
    int result;

    while ((result = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        const unsigned char* username = sqlite3_column_text(stmt, 0);
        const unsigned char* role = sqlite3_column_text(stmt, 1);

        std::cout << "Username: "
                  << (username ? reinterpret_cast<const char*>(username) : "")
                  << "\n";

        std::cout << "Role: "
                  << (role ? reinterpret_cast<const char*>(role) : "")
                  << "\n\n";

        count++;
    }

    if (result != SQLITE_DONE)
    {
        std::cout << "Error reading collaborators: "
                  << sqlite3_errmsg(db) << "\n";
    }
    else if (count == 0)
    {
        std::cout << "No collaborators found.\n";
    }

    sqlite3_finalize(stmt);
}
// Login an existing user
bool Member3::login(
    const std::string& username,
    const std::string& password
)
{
    if (db == nullptr) {
        std::cout << "Database is not connected!\n";
        return false;
    }

    if (username.empty() || password.empty()) {
        std::cout << "Username and password are required!\n";
        return false;
    }

    const char* sql =
        "SELECT id, password_hash, password_salt "
        "FROM users WHERE username = ?;";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cout << "Login query failed!\n";
        return false;
    }

    sqlite3_bind_text(
        stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT
    );

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        std::cout << "Invalid username or password!\n";
        return false;
    }

    int userId = sqlite3_column_int(stmt, 0);

    const unsigned char* storedHash = sqlite3_column_text(stmt, 1);
    const unsigned char* storedSalt = sqlite3_column_text(stmt, 2);

    if (storedHash == nullptr || storedSalt == nullptr) {
        sqlite3_finalize(stmt);
        return false;
    }

    std::string hashText =
        reinterpret_cast<const char*>(storedHash);

    std::string saltText =
        reinterpret_cast<const char*>(storedSalt);

    sqlite3_finalize(stmt);

    if (!PasswordHasher::verify(
        password,
        hashText,
        saltText
    )) {
    std::cout << "Invalid username or password!\n";
    return false;
}

    currentUserId = userId;
    currentUsername = username;

    std::cout << "Login successful! Welcome, "
              << currentUsername << "!\n";

    return true;
}
// Logout the currently logged-in user
void Member3::logout()
{
    if (currentUserId == -1)
    {
        std::cout << "No user is logged in.\n";
        return;
    }

    currentUserId = -1;
    currentUsername = "";

    std::cout << "Logged out successfully!\n";
}

// Check whether a user is logged in
bool Member3::isLoggedIn() const
{
    return currentUserId != -1;
}

int Member3::getCurrentUserId() const {
    return currentUserId;
}

std::string Member3::getCurrentUsername() const {
    return currentUsername;
}
// Create a new repository for the logged-in user
bool Member3::createRepository(
    const std::string& repoName,
    const std::string& visibility
)
{
    if (db == nullptr) {
        std::cout << "Database is not connected!\n";
        return false;
    }

    if (!isLoggedIn()) {
        std::cout << "Please login first!\n";
        return false;
    }

    if (repoName.empty()) {
        std::cout << "Repository name cannot be empty!\n";
        return false;
    }

    if (visibility != "public" && visibility != "private") {
        std::cout << "Visibility must be public or private!\n";
        return false;
    }

    const char* sql =
        "INSERT INTO repository_metadata "
        "(name, owner_id, visibility) "
        "VALUES (?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cout << "Error preparing repository query: "
                  << sqlite3_errmsg(db) << "\n";
        return false;
    }

    sqlite3_bind_text(
        stmt, 1, repoName.c_str(), -1, SQLITE_TRANSIENT
    );

    sqlite3_bind_int(stmt, 2, currentUserId);

    sqlite3_bind_text(
        stmt, 3, visibility.c_str(), -1, SQLITE_TRANSIENT
    );

    bool success = sqlite3_step(stmt) == SQLITE_DONE;

    if (success) {
        std::cout << "Repository created successfully!\n";
        std::cout << "Repository ID: "
                  << sqlite3_last_insert_rowid(db) << "\n";
    }
    else {
        std::cout << "Failed to create repository: "
                  << sqlite3_errmsg(db) << "\n";
    }

    sqlite3_finalize(stmt);

    return success;
}
// Display repositories owned by the logged-in user
void Member3::viewRepositories()
{
    if (db == nullptr) {
        std::cout << "Database is not connected!\n";
        return;
    }

    if (!isLoggedIn()) {
        std::cout << "Please login first!\n";
        return;
    }

    const char* sql =
        "SELECT id, name, visibility "
        "FROM repository_metadata "
        "WHERE owner_id = ? "
        "ORDER BY id;";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cout << "Error retrieving repositories: "
                  << sqlite3_errmsg(db) << "\n";
        return;
    }

    sqlite3_bind_int(stmt, 1, currentUserId);

    std::cout << "\n===== YOUR REPOSITORIES =====\n";

    int count = 0;
    int result;

    while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
        int repoId = sqlite3_column_int(stmt, 0);

        const unsigned char* name = sqlite3_column_text(stmt, 1);
        const unsigned char* visibility = sqlite3_column_text(stmt, 2);

        std::cout << "\nRepository ID: " << repoId << "\n";

        std::cout << "Name: "
                  << (name ? reinterpret_cast<const char*>(name) : "")
                  << "\n";

        std::cout << "Visibility: "
                  << (visibility ? reinterpret_cast<const char*>(visibility) : "")
                  << "\n";

        count++;
    }

    if (result != SQLITE_DONE) {
        std::cout << "Error reading repositories: "
                  << sqlite3_errmsg(db) << "\n";
    }
    else if (count == 0) {
        std::cout << "No repositories found.\n";
    }

    sqlite3_finalize(stmt);
}

// Add a collaborator to a repository
bool Member3::addCollaborator(
    int repoId,
    const std::string& username,
    const std::string& role
)
{
    if (db == nullptr) {
        std::cout << "Database is not connected!\n";
        return false;
    }

    if (!isLoggedIn()) {
        std::cout << "Please login first!\n";
        return false;
    }

    if (role != "read" && role != "write" && role != "admin") {
        std::cout << "Invalid role! Choose read, write, or admin.\n";
        return false;
    }

    if (username.empty()) {
        std::cout << "Username cannot be empty!\n";
        return false;
    }

    sqlite3_stmt* stmt = nullptr;

    // Verify that the logged-in user owns the repository
    const char* ownerSql =
        "SELECT owner_id FROM repository_metadata WHERE id = ?;";

    if (sqlite3_prepare_v2(db, ownerSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cout << "Error checking repository ownership!\n";
        return false;
    }

    sqlite3_bind_int(stmt, 1, repoId);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        std::cout << "Repository not found!\n";
        return false;
    }

    int ownerId = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (ownerId != currentUserId) {
        std::cout << "Only the repository owner can add collaborators!\n";
        return false;
    }

    // Find the user to add
    const char* userSql =
        "SELECT id FROM users WHERE username = ?;";

    if (sqlite3_prepare_v2(db, userSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cout << "Error searching for user!\n";
        return false;
    }

    sqlite3_bind_text(
        stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT
    );

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        std::cout << "User not found! Register the user first.\n";
        return false;
    }

    int collaboratorId = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (collaboratorId == currentUserId) {
        std::cout << "The repository owner cannot be added as a collaborator!\n";
        return false;
    }

    // Add the user to the collaborators table
    const char* insertSql =
        "INSERT INTO collaborators (repository_id, user_id, role) "
        "VALUES (?, ?, ?);";

    if (sqlite3_prepare_v2(db, insertSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cout << "Error preparing collaborator query: "
                  << sqlite3_errmsg(db) << "\n";
        return false;
    }

    sqlite3_bind_int(stmt, 1, repoId);
    sqlite3_bind_int(stmt, 2, collaboratorId);
    sqlite3_bind_text(
        stmt, 3, role.c_str(), -1, SQLITE_TRANSIENT
    );

    bool success = sqlite3_step(stmt) == SQLITE_DONE;

    if (success) {
        std::cout << "Collaborator added successfully!\n";
        std::cout << "Username: " << username << "\n";
        std::cout << "Role: " << role << "\n";
    }
    else {
        std::cout << "Failed to add collaborator: "
                  << sqlite3_errmsg(db) << "\n";
    }

    sqlite3_finalize(stmt);

    return success;
}

// Update the role of an existing collaborator
bool Member3::updateCollaboratorRole(
    int repoId,
    const std::string& username,
    const std::string& newRole
)
{
    if (db == nullptr) {
        std::cout << "Database is not connected!\n";
        return false;
    }

    if (!isLoggedIn()) {
        std::cout << "Please login first!\n";
        return false;
    }

    // Validate the new role
    if (newRole != "read" &&
        newRole != "write" &&
        newRole != "admin") {
        std::cout << "Invalid role! Choose read, write, or admin.\n";
        return false;
    }

    if (username.empty()) {
        std::cout << "Username cannot be empty!\n";
        return false;
    }

    sqlite3_stmt* stmt = nullptr;

    // Check whether the logged-in user owns the repository
    const char* ownerSql =
        "SELECT owner_id FROM repository_metadata WHERE id = ?;";

    if (sqlite3_prepare_v2(db, ownerSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cout << "Error checking repository ownership!\n";
        return false;
    }

    sqlite3_bind_int(stmt, 1, repoId);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        std::cout << "Repository not found!\n";
        return false;
    }

    int ownerId = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (ownerId != currentUserId) {
        std::cout << "Only the repository owner can update collaborators!\n";
        return false;
    }

    // Update the collaborator's role
    const char* updateSql =
        "UPDATE collaborators "
        "SET role = ? "
        "WHERE repository_id = ? "
        "AND user_id = (SELECT id FROM users WHERE username = ?);";

    if (sqlite3_prepare_v2(db, updateSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cout << "Error preparing update query: "
                  << sqlite3_errmsg(db) << "\n";
        return false;
    }

    sqlite3_bind_text(
        stmt, 1, newRole.c_str(), -1, SQLITE_TRANSIENT
    );

    sqlite3_bind_int(stmt, 2, repoId);

    sqlite3_bind_text(
        stmt, 3, username.c_str(), -1, SQLITE_TRANSIENT
    );

    bool success = sqlite3_step(stmt) == SQLITE_DONE;

    if (success && sqlite3_changes(db) > 0) {
        std::cout << "Collaborator role updated successfully!\n";
        std::cout << "Username: " << username << "\n";
        std::cout << "New Role: " << newRole << "\n";
    }
    else {
        std::cout << "Collaborator not found or update failed!\n";
        success = false;
    }

    sqlite3_finalize(stmt);

    return success;
}

// Remove an existing collaborator from a repository
bool Member3::removeCollaborator(
    int repoId,
    const std::string& username
)
{
    if (db == nullptr) {
        std::cout << "Database is not connected!\n";
        return false;
    }

    if (!isLoggedIn()) {
        std::cout << "Please login first!\n";
        return false;
    }

    if (username.empty()) {
        std::cout << "Username cannot be empty!\n";
        return false;
    }

    sqlite3_stmt* stmt = nullptr;

    // Verify repository ownership
    const char* ownerSql =
        "SELECT owner_id FROM repository_metadata WHERE id = ?;";

    if (sqlite3_prepare_v2(db, ownerSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cout << "Error checking repository ownership!\n";
        return false;
    }

    sqlite3_bind_int(stmt, 1, repoId);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        std::cout << "Repository not found!\n";
        return false;
    }

    int ownerId = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (ownerId != currentUserId) {
        std::cout << "Only the repository owner can remove collaborators!\n";
        return false;
    }

    // Remove collaborator using username
    const char* deleteSql =
        "DELETE FROM collaborators "
        "WHERE repository_id = ? "
        "AND user_id = (SELECT id FROM users WHERE username = ?);";

    if (sqlite3_prepare_v2(db, deleteSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cout << "Error preparing delete query: "
                  << sqlite3_errmsg(db) << "\n";
        return false;
    }

    sqlite3_bind_int(stmt, 1, repoId);

    sqlite3_bind_text(
        stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT
    );

    bool success = sqlite3_step(stmt) == SQLITE_DONE;

    if (success && sqlite3_changes(db) > 0) {
        std::cout << "Collaborator removed successfully!\n";
        std::cout << "Username: " << username << "\n";
    }
    else {
        std::cout << "Collaborator not found or removal failed!\n";
        success = false;
    }

    sqlite3_finalize(stmt);

    return success;
}