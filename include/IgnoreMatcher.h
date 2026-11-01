#ifndef IGNOREMATCHER_H
#define IGNOREMATCHER_H

#include <filesystem>
#include <string>
#include <vector>

class IgnoreMatcher {
private:
    std::vector<std::string> builtInRules;
    std::vector<std::string> userRules;

    static bool matchesPath(
        const std::string& path,
        const std::string& rule
    );

    static bool matchesComponent(
        const std::string& path,
        const std::string& rule
    );

public:
    explicit IgnoreMatcher(
        const std::filesystem::path& repositoryRoot
    );

    bool isIgnored(
        const std::string& normalizedPath
    ) const;
};

#endif