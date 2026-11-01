#ifndef MEMBER3_H
#define MEMBER3_H

#include <sqlite3.h>
#include <string>

class Member3 {
private:
    sqlite3* db;
    int currentUserId = -1;
    std::string currentUsername;

public:
    Member3(const std::string& databasePath);
    ~Member3();

    Member3(const Member3&) = delete;
    Member3& operator=(const Member3&) = delete;

    bool initializeDatabase();
    bool registerUser(const std::string& username,
                  const std::string& email,
                  const std::string& password);
    bool login(const std::string& username,
                    const std::string& password);
void logout();

bool isLoggedIn() const;
int getCurrentUserId() const;
std::string getCurrentUsername() const;
bool createRepository(const std::string& repoName,
                      const std::string& visibility);
void viewRepositories();

bool addCollaborator(int repoId,
                     const std::string& username,
                     const std::string& role);

void viewCollaborators(int repoId);

bool updateCollaboratorRole(int repoId,
                            const std::string& username,
                            const std::string& newRole);

bool removeCollaborator(int repoId,
                        const std::string& username);
};

#endif