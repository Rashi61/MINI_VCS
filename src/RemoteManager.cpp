#include "RemoteManager.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace fs = std::filesystem;
using namespace std;

static vector<string> listFiles(const string& dir) {
    vector<string> out;
    if (!fs::exists(dir)) return out;
    for (auto& e : fs::directory_iterator(dir)) {
        if (!e.is_regular_file()) continue;
        string n = e.path().filename().string();
        if (!n.empty() && n[0] != '.') out.push_back(n);   
    }
    return out;
}

static bool sameContent(const fs::path& a, const fs::path& b) {
    if (fs::file_size(a) != fs::file_size(b)) return false;
    ifstream fa(a, ios::binary), fb(b, ios::binary);
    return equal(istreambuf_iterator<char>(fa), istreambuf_iterator<char>(),
                 istreambuf_iterator<char>(fb));
}

static int countMissing(const string& from, const string& to) {
    int n = 0;
    for (auto& name : listFiles(from))
        if (!fs::exists(fs::path(to) / name)) n++;
    return n;
}

RemoteManager::RemoteManager(string localDir, string remoteDir)
    : local_(move(localDir)), remote_(move(remoteDir)) {}

int RemoteManager::localCount() const  { return (int)listFiles(local_).size(); }
int RemoteManager::remoteCount() const { return (int)listFiles(remote_).size(); }
int RemoteManager::aheadCount() const  { return countMissing(local_, remote_); }
int RemoteManager::behindCount() const { return countMissing(remote_, local_); }

SyncResult RemoteManager::copyMissing(const string& from, const string& to, const char* verb) {
    SyncResult r;
    fs::create_directories(to);
    for (auto& name : listFiles(from)) {
        fs::path src = fs::path(from) / name, dst = fs::path(to) / name;
        if (!fs::exists(dst)) {
            fs::copy_file(src, dst);
            fs::last_write_time(dst, fs::last_write_time(src));  
            r.copied++;
        } else if (!sameContent(src, dst)) {
            r.conflicts++;                                         
        }
    }
    r.message = string(verb) + " " + to_string(r.copied) + " snapshot(s)";
    if (r.conflicts > 0) {
        r.ok = false;
        r.message += ". " + to_string(r.conflicts) + " conflict(s): same name but different content, skipped";
    } else if (r.copied == 0) {
        r.message = "Already up to date";
    }
    return r;
}

SyncResult RemoteManager::clone() {
    SyncResult r;
    if (!fs::exists(remote_)) {
        r.ok = false;
        r.message = "Remote repository not found: " + remote_;
        return r;
    }
    if (!listFiles(local_).empty()) {
        r.ok = false;
        r.message = "Local folder is not empty. Clone needs an empty folder; use Pull instead.";
        return r;
    }
    SyncResult c = copyMissing(remote_, local_, "Cloned");
    if (c.copied == 0 && c.conflicts == 0) c.message = "Cloned 0 snapshot(s) (remote is empty)";
    return c;
}

SyncResult RemoteManager::push() { return copyMissing(local_, remote_, "Pushed"); }
SyncResult RemoteManager::pull() { return copyMissing(remote_, local_, "Pulled"); }
