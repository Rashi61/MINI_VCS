#include <iostream>
#include <string>
#include <limits>
#include "Member3.h"

int main() {
    Member3 system("minivcs.db");

    if (!system.initializeDatabase()) {
        std::cout << "Database initialization failed!\n";
        return 1;
    }

    int choice;
    std::string username, email, password;
    std::string repoName, visibility;

    while (true) {
        std::cout << "\n===== MINIVCS AUTHENTICATION =====\n";
        std::cout << "1. Register User\n";
        std::cout << "2. Login\n";
        std::cout << "3. Logout\n";
        std::cout << "4. Check Login Status\n";
        std::cout << "5. Create Repository\n";
        std::cout << "6. View Repositories\n";
        std::cout << "7. Add Collaborator\n";
        std::cout << "8. Exit\n";

        std::cout << "Enter your choice: ";

        if (!(std::cin >> choice)) {
            std::cout << "Invalid input!\n";
            break;
        }

        std::cin.ignore(
            std::numeric_limits<std::streamsize>::max(),
            '\n'
        );

        switch (choice) {
            case 1:
                std::cout << "Enter username: ";
                std::getline(std::cin, username);

                std::cout << "Enter email: ";
                std::getline(std::cin, email);

                std::cout << "Enter password: ";
                std::getline(std::cin, password);

                system.registerUser(username, email, password);
                break;

            case 2:
                std::cout << "Enter username: ";
                std::getline(std::cin, username);

                std::cout << "Enter password: ";
                std::getline(std::cin, password);

                system.login(username, password);
                break;

            case 3:
                system.logout();
                break;

            case 4:
                if (system.isLoggedIn()) {
                    std::cout << "User is currently logged in!\n";
                } else {
                    std::cout << "No user is logged in!\n";
                }
                break;

            case 5:
            std::cout << "Enter repository name: ";
            std::getline(std::cin, repoName);

            std::cout << "Enter visibility (public/private): ";
            std::getline(std::cin, visibility);

                system.createRepository(repoName, visibility);
                break;

    case 6:
    system.viewRepositories();
    break;
    case 7: {
    int repoId;
    std::string username, role;

    std::cout << "Enter Repository ID: ";
    std::cin >> repoId;

    std::cout << "Enter Collaborator Username: ";
    std::cin >> username;

    std::cout << "Enter Role (read/write/admin): ";
    std::cin >> role;

    system.addCollaborator(repoId, username, role);

    break;
}
    case 8:
    std::cout << "Exiting MiniVCS...\n";
    return 0;

            default:
                std::cout << "Invalid choice!\n";
        }
    }

    return 0;
}