#pragma once
#include <set>
#include <string>
#include <vector>

struct SnapshotInfo {
    std::string name;
    std::string path;
    std::string date;
    int  ageDays = 0;
    long ageHours = 0;
    bool latest = false;
    bool protectedTip = false;
    bool expired = false;
    bool isCommit = false; 
    bool pruned = false;
};

class RetentionManager {
public:
    RetentionManager(std::string snapshotDir, std::string configFile);

    void setRetentionDays(int days);
    int  getRetentionDays() const;

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