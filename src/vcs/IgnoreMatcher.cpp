#include "IgnoreMatcher.h"

#include <cctype>
#include <fstream>

namespace {

std::string trim(std::string value) {
    while (!value.empty() &&
           std::isspace(
               static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }

    std::size_t start = 0;

    while (start < value.size() &&
           std::isspace(
               static_cast<unsigned char>(value[start]))) {
        ++start;
    }

    return value.substr(start);
}

}

IgnoreMatcher::IgnoreMatcher(
    const std::filesystem::path& repositoryRoot
)
    : builtInRules{
          ".minivcs",
          ".git",
          ".minivcsignore",
          ".vs",
          ".idea",
          ".vscode",
          "out",
          "build",
          "minivcs",
          "minivcs.exe"
      } {
    std::ifstream input(
        repositoryRoot / ".minivcsignore"
    );

    std::string line;

    while (std::getline(input, line)) {
        line = trim(line);

        if (line.empty() || line[0] == '#') {
            continue;
        }

        while (!line.empty() && line.back() == '/') {
            line.pop_back();
        }

        if (!line.empty()) {
            userRules.push_back(line);
        }
    }
}

bool IgnoreMatcher::matchesPath(
    const std::string& path,
    const std::string& rule
) {
    return path == rule ||
           (
               path.size() > rule.size() &&
               path.compare(0, rule.size(), rule) == 0 &&
               path[rule.size()] == '/'
           );
}

bool IgnoreMatcher::matchesComponent(
    const std::string& path,
    const std::string& rule
) {
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

bool IgnoreMatcher::isIgnored(
    const std::string& path
) const {
    const std::string first =
        path.substr(0, path.find('/'));

    if (first.rfind("cmake-build-", 0) == 0 ||
        first.rfind("build-", 0) == 0) {
        return true;
    }

    for (const auto& rule : builtInRules) {
        if (matchesPath(path, rule)) {
            return true;
        }
    }

    for (const auto& rule : userRules) {
        if (matchesPath(path, rule) ||
            (
                rule.find('/') == std::string::npos &&
                matchesComponent(path, rule)
            )) {
            return true;
        }
    }

    return false;
}