#include "RetentionManager.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace fs = std::filesystem;
using namespace std;

namespace {
bool readCommitTime(const fs::path& commitFile, time_t& out, string& text) {
    ifstream in(commitFile);
    string id, message, stamp;

    if (!(in >> quoted(id) >> quoted(message) >> quoted(stamp))) {
        return false;
    }

    tm parsed{};
    istringstream stream(stamp);
    stream >> get_time(&parsed, "%Y-%m-%d %H:%M:%S");

    if (stream.fail()) {
        return false;
    }

    parsed.tm_isdst = -1;
    const time_t t = mktime(&parsed);

    if (t == static_cast<time_t>(-1)) {
        return false;
    }

    out = t;
    text = stamp.substr(0, 16);
    return true;
}

bool isCommitDir(const fs::directory_entry& e) {
    error_code ec;
    const string name = e.path().filename().string();

    if (!e.is_directory(ec) || ec || name.empty() || name[0] == '.') {
        return false;
    }

    if (name.size() > 4 && name.compare(name.size() - 4, 4, ".tmp") == 0) {
        return false;
    }

    return fs::is_regular_file(e.path() / "commit.txt", ec) && !ec;
}
}

RetentionManager::RetentionManager(string snapshotDir, string configFile)
    : dir_(std::move(snapshotDir)), cfg_(std::move(configFile)) {
    error_code ec;
    fs::create_directories(dir_, ec);
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

void RetentionManager::setProtected(const vector<string>& names) {
    protected_ = set<string>(names.begin(), names.end());
}

vector<SnapshotInfo> RetentionManager::listSnapshots() const {
    vector<SnapshotInfo> out;
    const auto sysNow = chrono::system_clock::now();
    const auto fileNow = fs::file_time_type::clock::now();

    error_code listError;
    for (fs::directory_iterator it(dir_, listError), end;
         !listError && it != end; it.increment(listError)) {
        const auto& e = *it;
        const string name = e.path().filename().string();

        error_code ec;
        SnapshotInfo s;
        s.name = name;
        s.path = e.path().string();

        if (isCommitDir(e)) {
            s.isCommit = true;
            s.pruned = fs::exists(e.path() / "pruned", ec);

            time_t t = 0;
            string text;

            if (readCommitTime(e.path() / "commit.txt", t, text)) {
                s.ageHours = static_cast<long>(
                    chrono::duration_cast<chrono::hours>(
                        sysNow - chrono::system_clock::from_time_t(t))
                        .count());
                s.date = text;
            } else {
                const auto age =
                    fileNow - fs::last_write_time(e.path() / "commit.txt", ec);
                s.ageHours = static_cast<long>(
                    chrono::duration_cast<chrono::hours>(age).count());
            }
        } else if (e.is_regular_file(ec) && !ec && !name.empty() &&
                   name[0] != '.') {
            const auto age = fileNow - fs::last_write_time(e.path(), ec);
            s.ageHours = static_cast<long>(
                chrono::duration_cast<chrono::hours>(age).count());

            const time_t t = chrono::system_clock::to_time_t(
                sysNow - chrono::duration_cast<chrono::seconds>(age));
            tm local{};
#ifdef _WIN32
            localtime_s(&local, &t);
#else
            localtime_r(&t, &local);
#endif
            char buf[32];
            strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", &local);
            s.date = buf;
        } else {
            continue;
        }

        if (s.ageHours < 0) s.ageHours = 0;
        s.ageDays = static_cast<int>(s.ageHours / 24);
        out.push_back(s);
    }

    sort(out.begin(), out.end(), [](const SnapshotInfo& a, const SnapshotInfo& b) {
        if (a.ageHours != b.ageHours) return a.ageHours < b.ageHours;
        return a.name > b.name;
    });

    const long limit = static_cast<long>(getRetentionDays()) * 24;
    for (size_t i = 0; i < out.size(); i++) {
        out[i].latest = (i == 0);
        out[i].protectedTip = protected_.count(out[i].name) != 0;
        out[i].expired = !out[i].latest && !out[i].protectedTip &&
                         !out[i].pruned && out[i].ageHours >= limit;
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

        if (s.isCommit) {
            fs::remove_all(fs::path(s.path) / "files", ec);
            if (ec) continue;

            ofstream marker(fs::path(s.path) / "pruned");
            marker << "removed by retention policy\n";
            marker.close();
            if (!marker) continue;

            log << s.name << "|" << s.date << "\n";
            n++;
        } else if (fs::remove(s.path, ec)) {
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