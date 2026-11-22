#include "RemoteManager.h"
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

static void write(const string& path, const string& text) {
    fs::create_directories(fs::path(path).parent_path());
    ofstream(path) << text;
}

static string readFirstLine(const string& path) {
    string s;
    ifstream f(path);
    getline(f, s);
    return s;
}

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
    check(readFirstLine(remote + "/c2.txt") == "second", "remote c2.txt was NOT overwritten");

    cout << "Test 5: nested push (subfolder is auto-created)\n";
    {
        const string l = base + "/n5_local", rem = base + "/n5_remote";
        fs::create_directories(rem);
        write(l + "/file.txt", "top level");
        write(l + "/docs/guide.txt", "nested guide");
        RemoteManager nm(l, rem);
        check(nm.aheadCount() == 2, "2 files (including nested) are waiting to be pushed");
        auto nr = nm.push();
        check(nr.ok && nr.copied == 2, "push sent 2 files");
        check(fs::exists(rem + "/docs/guide.txt"), "remote/docs/guide.txt was created");
        check(readFirstLine(rem + "/docs/guide.txt") == "nested guide", "nested file content is correct");
        check(nm.push().message == "Already up to date", "second nested push says already up to date");
    }

    cout << "Test 6: nested pull (deeper folders)\n";
    {
        const string l = base + "/n6_local", rem = base + "/n6_remote";
        fs::create_directories(l);
        write(rem + "/docs/deep/more.txt", "deep file");
        RemoteManager nm(l, rem);
        check(nm.behindCount() == 1, "1 nested remote file is waiting to be pulled");
        auto nr = nm.pull();
        check(nr.ok && nr.copied == 1, "pull brought 1 file");
        check(fs::exists(l + "/docs/deep/more.txt"), "local/docs/deep/more.txt was created");
        check(readFirstLine(l + "/docs/deep/more.txt") == "deep file", "pulled nested content is correct");
    }

    cout << "Test 7: nested clone\n";
    {
        const string l = base + "/n7_local", rem = base + "/n7_remote";
        fs::create_directories(l);
        write(rem + "/file.txt", "top level");
        write(rem + "/docs/guide.txt", "nested guide");
        RemoteManager nm(l, rem);
        auto nr = nm.clone();
        check(nr.ok && nr.copied == 2, "clone copied 2 files");
        check(fs::exists(l + "/file.txt") && fs::exists(l + "/docs/guide.txt"),
              "both top-level and nested files exist in local");
        check(nm.localCount() == 2 && nm.remoteCount() == 2, "local and remote both have 2");
    }

    cout << "Test 8: nested conflict\n";
    {
        const string l = base + "/n8_local", rem = base + "/n8_remote";
        write(l + "/docs/guide.txt", "local version");
        write(rem + "/docs/guide.txt", "remote version");
        RemoteManager nm(l, rem);
        auto nr = nm.push();
        check(!nr.ok && nr.conflicts == 1, "push reports 1 nested conflict");
        check(readFirstLine(rem + "/docs/guide.txt") == "remote version",
              "remote nested file was NOT overwritten");
        nr = nm.pull();
        check(!nr.ok && nr.conflicts == 1, "pull reports 1 nested conflict");
        check(readFirstLine(l + "/docs/guide.txt") == "local version",
              "local nested file was NOT overwritten");
    }

    { std::error_code ec; fs::remove_all(base, ec); }
    cout << (failures ? "\nSOME TESTS FAILED\n" : "\nALL TESTS PASSED\n");
    return failures ? 1 : 0;
}