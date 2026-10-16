#include "retention/RetentionManager.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace std;

RetentionManager::RetentionManager(string snapshotDir, string configFile)
    : dir_(move(snapshotDir)), cfg_(move(configFile)) {
    fs::create_directories(dir_);
}

void RetentionManager::setRetentionDays(int days) {
    if (days < 0) days = 0;
    ofstream f(cfg_);
    f << "retention_days=" << days << "\n";
}

int RetentionManager::getRetentionDays() const {
    ifstream f(cfg_);
    string line;
    while (getline(f, line)) {
        const string key = "retention_days=";
        if (line.rfind(key, 0) == 0) return max(0, atoi(line.c_str() + key.size()));
    }
    return 30; 
}

vector<SnapshotInfo> RetentionManager::listSnapshots() const {
    vector<SnapshotInfo> out;
    auto fileNow = fs::file_time_type::clock::now();
    auto sysNow = chrono::system_clock::now();

    for (auto& e : fs::directory_iterator(dir_)) {
        if (!e.is_regular_file()) continue;
        string name = e.path().filename().string();
        if (name.empty() || name[0] == '.') continue;   

        SnapshotInfo s;
        s.name = name;
        s.path = e.path().string();

        auto age = fileNow - fs::last_write_time(e.path());
        s.ageHours = (long)chrono::duration_cast<chrono::hours>(age).count();
        if (s.ageHours < 0) s.ageHours = 0;
        s.ageDays = (int)(s.ageHours / 24);

        time_t t = chrono::system_clock::to_time_t(
            sysNow - chrono::duration_cast<chrono::seconds>(age));
        char buf[32];
        strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", localtime(&t));
        s.date = buf;
        out.push_back(s);
    }
    
    sort(out.begin(), out.end(), [](const SnapshotInfo& a, const SnapshotInfo& b) {
        if (a.ageHours != b.ageHours) return a.ageHours < b.ageHours;
        return a.name > b.name;
    });

    long limit = (long)getRetentionDays() * 24;
    for (size_t i = 0; i < out.size(); i++) {
        out[i].latest = (i == 0);
        out[i].expired = (i != 0) && (out[i].ageHours >= limit);  
    }
    return out;
}

vector<SnapshotInfo> RetentionManager::findExpired() const {
    vector<SnapshotInfo> out;
    for (auto& s : listSnapshots())
        if (s.expired) out.push_back(s);
    return out;
}

int RetentionManager::cleanup() {
    auto list = findExpired();
    ofstream log(dir_ + "/.expired_log", ios::app);
    int n = 0;
    for (auto& s : list) {
        error_code ec;
        if (fs::remove(s.path, ec)) {
            log << s.name << "|" << s.date << "\n";
            n++;
        }
    }
    return n;
}

vector<string> RetentionManager::expiredLog() const {
    vector<string> out;
    ifstream f(dir_ + "/.expired_log");
    string line;
    while (getline(f, line))
        if (!line.empty()) out.push_back(line);
    return out;
}
