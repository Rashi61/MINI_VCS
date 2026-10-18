#pragma once
#include <set>
#include <string>
#include <vector>

struct SnapshotInfo {
    std::string name;     // file name, or commit ID for a commit folder
    std::string path;     // full path
    std::string date;     // "YYYY-MM-DD HH:MM"
    int  ageDays = 0;
    long ageHours = 0;
    bool latest = false;       // newest entry: never expires
    bool protectedTip = false; // named via setProtected(): never expires
    bool expired = false;
    bool isCommit = false;     // a MiniVCS commit folder (contains commit.txt)
    bool pruned = false;       // stored files already removed, history kept
};

// Retention policy for a folder of snapshots. Works on both plain files
// and MiniVCS commit folders (.minivcs/objects/<id>/).
//
// For a commit folder, "removing" means deleting only its stored file copies
// (objects/<id>/files) and writing a 'pruned' marker. commit.txt stays, so
// history, parent links and branch pointers remain valid.
class RetentionManager {
public:
    RetentionManager(std::string snapshotDir, std::string configFile);

    void setRetentionDays(int days);
    int  getRetentionDays() const;

    // Names that must never expire (for example every branch tip).
    void setProtected(const std::vector<std::string>& names);

    std::vector<SnapshotInfo> listSnapshots() const;
    std::vector<SnapshotInfo> findExpired() const;
    int cleanup();

    std::vector<std::string> expiredLog() const;

private:
    std::string dir_;
    std::string cfg_;
    std::set<std::string> protected_;
};