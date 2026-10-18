#include "DiffManager.h"

#include "CommitManager.h"
#include "FileManager.h"
#include "HashManager.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace {

namespace fs = std::filesystem;

// Above this many line pairs we refuse to build the comparison table.
constexpr std::size_t kMaxTableCells = 4000000;
constexpr std::size_t kContext = 3;

struct Op {
    char tag;           // ' ' same, '-' removed, '+' added
    std::string text;
};

// One side of a file comparison.
struct Side {
    bool exists = false;
    fs::path file;
};

enum class ReadResult { Ok, Binary, Unreadable };

ReadResult readLines(const fs::path& path, std::vector<std::string>& lines) {
    lines.clear();

    std::ifstream input(path, std::ios::binary);

    if (!input) {
        return ReadResult::Unreadable;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string data = buffer.str();

    if (data.find('\0') != std::string::npos) {
        return ReadResult::Binary;
    }

    std::size_t start = 0;

    while (start < data.size()) {
        std::size_t end = data.find('\n', start);

        if (end == std::string::npos) {
            end = data.size();
        }

        lines.push_back(data.substr(start, end - start));
        start = end + 1;
    }

    return ReadResult::Ok;
}

// Builds the edit script using a longest-common-subsequence table.
bool buildOps(const std::vector<std::string>& a,
              const std::vector<std::string>& b,
              std::vector<Op>& ops) {
    ops.clear();

    std::size_t prefix = 0;
    while (prefix < a.size() && prefix < b.size() && a[prefix] == b[prefix]) {
        ++prefix;
    }

    std::size_t suffix = 0;
    while (suffix < a.size() - prefix && suffix < b.size() - prefix &&
           a[a.size() - 1 - suffix] == b[b.size() - 1 - suffix]) {
        ++suffix;
    }

    const std::size_t n = a.size() - prefix - suffix;
    const std::size_t m = b.size() - prefix - suffix;

    if ((n + 1) * (m + 1) > kMaxTableCells) {
        return false;
    }

    for (std::size_t i = 0; i < prefix; ++i) {
        ops.push_back({' ', a[i]});
    }

    // lcs[i][j] = LCS length of a[prefix+i..] and b[prefix+j..]
    std::vector<std::uint32_t> lcs((n + 1) * (m + 1), 0);
    auto at = [&](std::size_t i, std::size_t j) -> std::uint32_t& {
        return lcs[i * (m + 1) + j];
    };

    for (std::size_t i = n; i-- > 0;) {
        for (std::size_t j = m; j-- > 0;) {
            at(i, j) = (a[prefix + i] == b[prefix + j])
                           ? at(i + 1, j + 1) + 1
                           : std::max(at(i + 1, j), at(i, j + 1));
        }
    }

    std::size_t i = 0, j = 0;

    while (i < n && j < m) {
        if (a[prefix + i] == b[prefix + j]) {
            ops.push_back({' ', a[prefix + i]});
            ++i;
            ++j;
        } else if (at(i + 1, j) >= at(i, j + 1)) {
            ops.push_back({'-', a[prefix + i]});
            ++i;
        } else {
            ops.push_back({'+', b[prefix + j]});
            ++j;
        }
    }

    for (; i < n; ++i) {
        ops.push_back({'-', a[prefix + i]});
    }

    for (; j < m; ++j) {
        ops.push_back({'+', b[prefix + j]});
    }

    for (std::size_t k = a.size() - suffix; k < a.size(); ++k) {
        ops.push_back({' ', a[k]});
    }

    return true;
}

Side commitSide(const CommitManager& manager, const std::string& commitId,
                const std::string& path, bool inCommit) {
    Side side;

    if (inCommit) {
        side.exists = true;
        side.file = manager.getSnapshotPath(commitId) / path;
    }

    return side;
}

// Writes the diff of one file. Returns false only on an unreadable file.
bool writeFileDiff(std::ostream& out, const std::string& path,
                   const Side& before, const Side& after) {
    std::vector<std::string> oldLines, newLines;

    const ReadResult oldResult =
        before.exists ? readLines(before.file, oldLines) : ReadResult::Ok;
    const ReadResult newResult =
        after.exists ? readLines(after.file, newLines) : ReadResult::Ok;

    if (oldResult == ReadResult::Unreadable ||
        newResult == ReadResult::Unreadable) {
        std::cerr << "Could not read file for diff: " << path << '\n';
        return false;
    }

    out << "diff --minivcs a/" << path << " b/" << path << '\n';

    if (!before.exists) {
        out << "new file\n";
    } else if (!after.exists) {
        out << "deleted file\n";
    }

    if (oldResult == ReadResult::Binary || newResult == ReadResult::Binary) {
        out << "Binary files differ\n";
        return true;
    }

    bool tooLarge = false;
    const std::string body =
        DiffManager::diffLines(oldLines, newLines, tooLarge);

    if (tooLarge) {
        out << "File is too large to compare line by line\n";
        return true;
    }

    out << "--- " << (before.exists ? "a/" + path : std::string("/dev/null"))
        << '\n'
        << "+++ " << (after.exists ? "b/" + path : std::string("/dev/null"))
        << '\n'
        << body;

    return true;
}

}  // namespace

DiffManager::DiffManager(const std::filesystem::path& rootPath)
    : repositoryRoot(std::filesystem::absolute(rootPath)) {}

std::string DiffManager::diffLines(const std::vector<std::string>& oldLines,
                                   const std::vector<std::string>& newLines,
                                   bool& tooLarge) {
    tooLarge = false;

    std::vector<Op> ops;

    if (!buildOps(oldLines, newLines, ops)) {
        tooLarge = true;
        return "";
    }

    // Positions: consumedOld/New = lines before op k.
    std::vector<std::size_t> oldBefore(ops.size() + 1, 0);
    std::vector<std::size_t> newBefore(ops.size() + 1, 0);

    for (std::size_t k = 0; k < ops.size(); ++k) {
        oldBefore[k + 1] = oldBefore[k] + (ops[k].tag != '+' ? 1 : 0);
        newBefore[k + 1] = newBefore[k] + (ops[k].tag != '-' ? 1 : 0);
    }

    std::ostringstream out;
    std::size_t k = 0;

    while (k < ops.size()) {
        // Find the next change.
        while (k < ops.size() && ops[k].tag == ' ') {
            ++k;
        }

        if (k >= ops.size()) {
            break;
        }

        const std::size_t lo = (k > kContext) ? k - kContext : 0;

        // Extend the hunk while further changes are within 2*context.
        std::size_t end = k;
        std::size_t lastChange = k;

        while (end < ops.size()) {
            if (ops[end].tag != ' ') {
                lastChange = end;
            } else if (end - lastChange > 2 * kContext) {
                break;
            }
            ++end;
        }

        const std::size_t hi = std::min(ops.size(), lastChange + kContext + 1);

        std::size_t oldCount = 0, newCount = 0;
        for (std::size_t q = lo; q < hi; ++q) {
            oldCount += (ops[q].tag != '+');
            newCount += (ops[q].tag != '-');
        }

        const std::size_t oldStart =
            oldCount == 0 ? oldBefore[lo] : oldBefore[lo] + 1;
        const std::size_t newStart =
            newCount == 0 ? newBefore[lo] : newBefore[lo] + 1;

        out << "@@ -" << oldStart << ',' << oldCount << " +" << newStart
            << ',' << newCount << " @@\n";

        for (std::size_t q = lo; q < hi; ++q) {
            // Show Windows (CRLF) files without a stray carriage return.
            std::string shown = ops[q].text;
            if (!shown.empty() && shown.back() == '\r') {
                shown.pop_back();
            }
            out << ops[q].tag << shown << '\n';
        }

        k = hi;
    }

    return out.str();
}

bool DiffManager::showCommit(const std::string& commitId,
                             std::ostream& out) const {
    CommitManager manager(repositoryRoot);
    Commit commit;

    if (!manager.getCommit(commitId, commit)) {
        return false;
    }

    out << "Commit:   " << commit.getId() << '\n';
    out << "Message:  " << commit.getMessage() << '\n';
    out << "Date:     " << commit.getTimestamp() << '\n';
    out << "Parent:   "
        << (commit.getParentId().empty() ? std::string("none")
                                         : commit.getParentId())
        << '\n';
    out << "Snapshot: "
        << (manager.isSnapshotPruned(commit.getId())
                ? "removed by retention policy"
                : "available")
        << '\n';

    std::vector<std::string> paths;
    for (const auto& entry : commit.getFileHashes()) {
        paths.push_back(entry.first);
    }
    std::sort(paths.begin(), paths.end());

    out << "Files (" << paths.size() << "):\n";
    for (const auto& path : paths) {
        out << "  " << commit.getFileHashes().at(path).substr(0, 12)
            << "  " << path << '\n';
    }

    return true;
}

bool DiffManager::diffWorkingTree(const std::string& commitId,
                                  std::ostream& out) const {
    CommitManager manager(repositoryRoot);
    Commit commit;

    if (commitId.empty()) {
        const auto history = manager.getCommitHistory();

        if (history.empty()) {
            out << "No commits yet.\n";
            return true;
        }

        commit = history.back();
    } else if (!manager.getCommit(commitId, commit)) {
        return false;
    }

    if (manager.isSnapshotPruned(commit.getId())) {
        std::cerr << "The snapshot of this commit was removed by the "
                     "retention policy; it cannot be compared.\n";
        return false;
    }

    FileManager files(repositoryRoot);
    std::set<std::string> paths;

    for (const auto& path : files.getTrackedFiles()) {
        paths.insert(path);
    }

    for (const auto& entry : commit.getFileHashes()) {
        paths.insert(entry.first);
    }

    const auto& committed = commit.getFileHashes();
    bool anyDifference = false;

    for (const auto& path : paths) {
        const auto found = committed.find(path);
        const bool inCommit = found != committed.end();

        std::error_code ec;
        const fs::path working = repositoryRoot / path;
        const bool exists = fs::is_regular_file(working, ec) && !ec;

        if (!inCommit && !exists) {
            continue;
        }

        if (inCommit && exists &&
            HashManager::calculateHash(working) == found->second) {
            continue;
        }

        Side after;
        after.exists = exists;
        after.file = working;

        if (!writeFileDiff(out, path,
                           commitSide(manager, commit.getId(), path,
                                      inCommit),
                           after)) {
            return false;
        }

        anyDifference = true;
    }

    if (!anyDifference) {
        out << "No differences.\n";
    }

    return true;
}

bool DiffManager::diffCommits(const std::string& fromId,
                              const std::string& toId,
                              std::ostream& out) const {
    CommitManager manager(repositoryRoot);
    Commit from, to;

    if (!manager.getCommit(fromId, from) || !manager.getCommit(toId, to)) {
        return false;
    }

    if (manager.isSnapshotPruned(from.getId()) ||
        manager.isSnapshotPruned(to.getId())) {
        std::cerr << "A selected commit's snapshot was removed by the "
                     "retention policy; it cannot be compared.\n";
        return false;
    }

    std::set<std::string> paths;

    for (const auto& entry : from.getFileHashes()) {
        paths.insert(entry.first);
    }

    for (const auto& entry : to.getFileHashes()) {
        paths.insert(entry.first);
    }

    bool anyDifference = false;

    for (const auto& path : paths) {
        const auto a = from.getFileHashes().find(path);
        const auto b = to.getFileHashes().find(path);
        const bool inFrom = a != from.getFileHashes().end();
        const bool inTo = b != to.getFileHashes().end();

        if (inFrom && inTo && a->second == b->second) {
            continue;
        }

        if (!writeFileDiff(out, path,
                           commitSide(manager, from.getId(), path, inFrom),
                           commitSide(manager, to.getId(), path, inTo))) {
            return false;
        }

        anyDifference = true;
    }

    if (!anyDifference) {
        out << "No differences.\n";
    }

    return true;
}