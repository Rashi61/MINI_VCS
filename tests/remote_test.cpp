#include "remote/RemoteManager.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using namespace std;

static int failures = 0;
static void check(bool cond, const string& what) {
    cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
    if (!cond) failures++;
}
static void write(const string& path, const string& text) { ofstream(path) << text; }

int main() {
    const string base = "tests/tmp_remote";
    const string local = base + "/local", remote = base + "/remote";
    { std::error_code ec; fs::remove_all(base, ec); }
    fs::create_directories(remote);
    fs::create_directories(local);

    write(remote + "/c1.txt", "first");
    write(remote + "/c2.txt", "second");
    write(remote + "/c3.txt", "third");
    RemoteManager rm(local, remote);

    cout << "Test 1: clone\n";
    auto r = rm.clone();
    check(r.ok && r.copied == 3, "clone copied 3 snapshots to local");
    check(rm.localCount() == 3 && rm.remoteCount() == 3, "local and remote both have 3");
    check(!rm.clone().ok, "clone into a non-empty local folder is refused");

    cout << "Test 2: push\n";
    write(local + "/c4.txt", "fourth (local only)");
    check(rm.aheadCount() == 1, "1 local snapshot is waiting to be pushed");
    r = rm.push();
    check(r.ok && r.copied == 1, "push sent 1 snapshot");
    check(fs::exists(remote + "/c4.txt"), "c4.txt now exists in remote");
    check(rm.push().message == "Already up to date", "second push says already up to date");

    cout << "Test 3: pull\n";
    write(remote + "/c5.txt", "fifth (remote only)");
    check(rm.behindCount() == 1, "1 remote snapshot is waiting to be pulled");
    r = rm.pull();
    check(r.ok && r.copied == 1 && fs::exists(local + "/c5.txt"), "pull brought c5.txt into local");

    cout << "Test 4: conflict (same name, different content)\n";
    write(local + "/c2.txt", "second EDITED locally");
    r = rm.push();
    check(!r.ok && r.conflicts == 1, "push reports 1 conflict");
    check(ifstream(remote + "/c2.txt").good(), "remote c2.txt still exists");
    string content;
    { ifstream f(remote + "/c2.txt"); getline(f, content); }
    check(content == "second", "remote c2.txt was NOT overwritten");

    { std::error_code ec; fs::remove_all(base, ec); }
    cout << (failures ? "\nSOME TESTS FAILED\n" : "\nALL TESTS PASSED\n");
    return failures ? 1 : 0;
}
