#ifndef PASSWORD_HASHER_H
#define PASSWORD_HASHER_H

#include <string>

struct PasswordHash {
    std::string hash;
    std::string salt;
};

class PasswordHasher {
public:
    static bool createHash(
        const std::string& password,
        PasswordHash& result
    );

    static bool verify(
        const std::string& password,
        const std::string& storedHash,
        const std::string& storedSalt
    );
};

#endif