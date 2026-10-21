#!/usr/bin/env python3
"""MiniVCS end-to-end tests (Windows, Linux and macOS; no bash needed).

Usage (from the repository root):

    python tests/run_tests.py                      # finds the built binary
    python tests/run_tests.py build/Debug/minivcs.exe
    python tests/run_tests.py C:\\path\\to\\minivcs.exe

The binary is searched in the usual CMake locations (build/, build/Debug/,
build/Release/, cmake-build-*/ ...). If nothing is found the script tries a
fresh CMake build. Set MINIVCS_BIN to point at a binary explicitly.
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EXE_NAMES = ["minivcs.exe", "minivcs"]

passed = failed = skipped = 0


# ----------------------------------------------------------------- helpers
def ok(name):
    global passed
    passed += 1
    print(f"  PASS  {name}")


def bad(name, detail=""):
    global failed
    failed += 1
    print(f"  FAIL  {name}")
    for line in str(detail).strip().splitlines()[:8]:
        print(f"        {line}")


def skip(name):
    global skipped
    skipped += 1
    print(f"  SKIP  {name}")


def check(name, condition, detail=""):
    if condition:
        ok(name)
    else:
        bad(name, detail)


def find_binary(argument):
    candidates = []

    if argument:
        candidates.append(Path(argument))
    if os.environ.get("MINIVCS_BIN"):
        candidates.append(Path(os.environ["MINIVCS_BIN"]))

    folders = [ROOT / "build", ROOT / "build" / "Debug",
               ROOT / "build" / "Release", ROOT / "out" / "build",
               ROOT, ROOT / "Debug", ROOT / "Release"]
    folders += sorted(ROOT.glob("cmake-build-*"))
    folders += [d / sub for d in sorted(ROOT.glob("cmake-build-*"))
                for sub in ("Debug", "Release")]
    folders += sorted(ROOT.glob("build*/Debug")) + sorted(ROOT.glob("build*/Release"))
    folders += sorted(ROOT.glob("build*"))

    for folder in folders:
        for name in EXE_NAMES:
            candidates.append(folder / name)

    for candidate in candidates:
        for variant in (candidate, candidate.with_suffix(".exe")):
            if variant.is_file():
                return variant.resolve()
    return None


def build_binary():
    build_dir = ROOT / "build-tests"
    print(f"No binary found. Building with CMake in {build_dir} ...")
    for command in (["cmake", "-S", str(ROOT), "-B", str(build_dir)],
                    ["cmake", "--build", str(build_dir), "--config", "Debug"]):
        try:
            result = subprocess.run(command, capture_output=True, text=True)
        except FileNotFoundError:
            return None
        if result.returncode != 0:
            print(result.stdout[-1500:], result.stderr[-1500:])
            return None
    for folder in (build_dir, build_dir / "Debug", build_dir / "Release"):
        for name in EXE_NAMES:
            if (folder / name).is_file():
                return (folder / name).resolve()
    return None


BIN = None
CWD = None


def run(*args, cwd=None):
    """Runs minivcs. Returns (exit_code, stdout+stderr as text)."""
    result = subprocess.run(
        [str(BIN), *[str(a) for a in args]],
        cwd=str(cwd or CWD), capture_output=True,
        text=True, encoding="utf-8", errors="replace")
    return result.returncode, (result.stdout or "") + (result.stderr or "")


def write(path, text):
    """Writes with plain \\n line endings on every platform."""
    Path(CWD, path).parent.mkdir(parents=True, exist_ok=True)
    Path(CWD, path).write_bytes(text.encode("utf-8"))


def read(path):
    return Path(CWD, path).read_bytes().decode("utf-8", "replace")


def latest():
    return read(".minivcs/branches/main").strip()


def expect_ok(name, *args):
    code, out = run(*args)
    check(name, code == 0, f"exit {code}\n{out}")


def expect_fail(name, *args):
    code, out = run(*args)
    check(name, code != 0, f"unexpected success\n{out}")


def expect_out(name, text, *args):
    _, out = run(*args)
    check(name, text in out, f"wanted: {text}\n{out}")


def expect_no_out(name, text, *args):
    _, out = run(*args)
    check(name, text not in out, f"found: {text}\n{out}")


def rewrite_commit_time(commit_id, stamp="2026-01-01 10:00:00"):
    """Replaces the stored timestamp (3rd line) of a commit, keeping the
    original line endings."""
    path = Path(CWD, ".minivcs", "objects", commit_id, "commit.txt")
    with open(path, encoding="utf-8", newline="") as handle:
        lines = handle.read().splitlines(keepends=True)
    ending = "\r\n" if lines[2].endswith("\r\n") else "\n"
    lines[2] = f'"{stamp}"{ending}'
    with open(path, "w", encoding="utf-8", newline="") as handle:
        handle.write("".join(lines))


def fresh_repo(parent, name):
    global CWD
    CWD = Path(parent, name)
    shutil.rmtree(CWD, ignore_errors=True)
    CWD.mkdir(parents=True)
    run("init")


# ------------------------------------------------------------------- tests
def main():
    global BIN, CWD

    argument = sys.argv[1] if len(sys.argv) > 1 else None
    BIN = find_binary(argument)
    if BIN is None and argument is None:
        BIN = build_binary()
    if BIN is None:
        print("Could not find or build minivcs.\n"
              "Build it first (cmake -S . -B build && cmake --build build),\n"
              "then run:  python tests/run_tests.py <path to minivcs.exe>")
        return 2

    print(f"Using binary: {BIN}\n")
    work = Path(tempfile.mkdtemp(prefix="minivcs-tests-"))

    try:
        run_all(work)
    finally:
        os.chdir(ROOT)
        shutil.rmtree(work, ignore_errors=True)

    print(f"\nPassed: {passed}   Failed: {failed}   Skipped: {skipped}")
    return 0 if failed == 0 else 1


def run_all(work):
    global CWD

    print("== 1. Initialize a fresh repository ==")
    fresh_repo(work, "main")
    check("metadata directories exist",
          (CWD / ".minivcs/objects").is_dir() and (CWD / ".minivcs/branches").is_dir())
    check("HEAD file exists", (CWD / ".minivcs/HEAD").is_file())
    check("branches/main created by init", (CWD / ".minivcs/branches/main").is_file())
    expect_out("second init is harmless", "already initialized", "init")

    print("== 2. Track a file ==")
    write("a.txt", "one\n"); write("b.txt", "bee\n"); write("sub/s.txt", "s\n")
    expect_ok("add a.txt", "add", "a.txt")
    expect_ok("add b.txt", "add", "b.txt")
    expect_ok("add sub/s.txt", "add", "sub/s.txt")
    expect_out("tracked lists a.txt", "a.txt", "tracked")
    expect_fail("add rejects ../outside", "add", "../outside.txt")
    expect_fail("add rejects .minivcs/HEAD", "add", ".minivcs/HEAD")
    expect_fail("add rejects missing file", "add", "nope.txt")

    print("== 3. Commit ==")
    write("c.txt", "c\n")
    expect_out("uncommitted files reported", "Uncommitted: a.txt", "status")
    expect_ok("first commit", "commit", "first", "commit")
    c1 = latest()
    check("commit object stored", (CWD / f".minivcs/objects/{c1}/commit.txt").is_file())
    check("snapshot copy stored", (CWD / f".minivcs/objects/{c1}/files/a.txt").is_file())
    expect_fail("empty message rejected", "commit", "   ")
    expect_out("no-change commit refused", "Nothing to commit", "commit", "again")
    expect_out("untracked file reported", "Untracked: c.txt", "status")

    print("== 4. Modify and detect ==")
    write("a.txt", "two\n")
    expect_out("modified file detected", "Modified: a.txt", "status")

    print("== 5. Missing and untracked ==")
    (CWD / "b.txt").unlink()
    expect_out("missing file detected", "Missing: b.txt", "status")
    expect_out("untracked file detected", "Untracked: c.txt", "status")
    expect_fail("commit refuses missing file", "commit", "broken")
    write("b.txt", "bee\n")
    expect_ok("second commit", "commit", "second", "commit")
    c2 = latest()
    check("branch pointer moved", c1 != c2)

    print("== 6. History and parent links ==")
    _, hist = run("history")
    check("history lists both commits", c1 in hist and c2 in hist, hist)
    check("newest commit listed first", hist.find(c2) < hist.find(c1), hist)
    check("second commit's parent is first", f"Parent: {c1}" in hist, hist)
    check("first commit has no parent", "Parent: none" in hist, hist)
    check("parent id stored in commit.txt",
          f'"{c1}"' in read(f".minivcs/objects/{c2}/commit.txt"))

    print("== 7. Restore ==")
    write("a.txt", "unsaved\n")
    expect_out("restore refused on local edits", "uncommitted", "restore", c1)
    check("unsaved work preserved", read("a.txt").strip() == "unsaved")
    expect_ok("restore --force works", "restore", c1, "--force")
    check("a.txt content restored", read("a.txt").strip() == "one")
    check("branch pointer unchanged", latest() == c2)
    write("a.txt", "two\n")
    expect_ok("restore on clean tree works", "restore", c1)
    expect_ok("restore by 8-char prefix", "restore", c2[:8])
    check("prefix restore content", read("a.txt").strip() == "two")
    expect_fail("too-short prefix rejected", "restore", "abc")
    expect_fail("unknown id rejected", "restore", "0000deadbeef")
    expect_fail("path-like id rejected", "restore", "../../etc")
    with open(CWD / f".minivcs/objects/{c1}/files/a.txt", "ab") as handle:
        handle.write(b"X\n")
    expect_out("corrupt snapshot detected", "corrupted", "restore", c1, "--force")
    check("no staging dir left behind", not (CWD / ".minivcs/restore-staging").exists())

    print("== Ignore rules ==")
    write("tmpdir/t.txt", "t\n"); write(".minivcsignore", "tmpdir/\n")
    expect_no_out("ignored directory not listed", "tmpdir", "status")
    write("cmake-build-debug/x.obj", "o\n"); write("build-x64/y.obj", "o\n")
    expect_no_out("IDE build folders not listed", "build-", "status")

    print("== show ==")
    fresh_repo(work, "show")
    write("a.txt", "l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\n"); write("k.txt", "keep\n")
    run("add", "a.txt"); run("add", "k.txt"); run("commit", "one")
    s1 = latest()
    write("a.txt", "l1\nl2 CHANGED\nl3\nl4\nl5\nl6\nl7\nl8\nl9\n")
    run("remove", "k.txt"); write("n.txt", "new\n"); run("add", "n.txt")
    run("commit", "two")
    s2 = latest()
    expect_out("show prints full id", s2, "show", s2[:8])
    expect_out("show prints message", "Message:  two", "show", s2[:8])
    expect_out("show prints parent", f"Parent:   {s1}", "show", s2[:8])
    expect_out("show lists files", "n.txt", "show", s2[:8])
    expect_out("show reports snapshot state", "Snapshot: available", "show", s1)
    expect_fail("show unknown commit fails", "show", "0000deadbeef")
    expect_fail("show needs an argument", "show")

    print("== diff ==")
    expect_out("diff commits: changed line", "+l2 CHANGED", "diff", s1[:8], s2[:8])
    expect_out("diff commits: removed line", "-l2", "diff", s1[:8], s2[:8])
    expect_out("diff commits: added line", "+l9", "diff", s1[:8], s2[:8])
    expect_out("diff commits: new file", "new file", "diff", s1[:8], s2[:8])
    expect_out("diff commits: deleted file", "deleted file", "diff", s1[:8], s2[:8])
    expect_out("diff commits: hunk header", "@@ -", "diff", s1[:8], s2[:8])
    expect_out("identical commits", "No differences", "diff", s2[:8], s2[:8])
    expect_out("clean tree has no diff", "No differences", "diff")
    write("a.txt", "l1\nl2 CHANGED\nWORKING EDIT\n")
    expect_out("working tree edit shown", "+WORKING EDIT", "diff")
    expect_out("diff against older commit", "-l2", "diff", s1[:8])
    (CWD / "n.txt").unlink()
    expect_out("missing tracked file shown", "deleted file", "diff")
    write("bin.dat", "a\0b"); run("add", "bin.dat"); write("bin.dat", "a\0c")
    expect_out("binary change handled", "Binary files differ", "diff")
    expect_fail("diff unknown commit fails", "diff", "0000deadbeef")
    expect_fail("too many diff arguments", "diff", "a", "b", "c")

    print("== Retention (commit folders) ==")
    fresh_repo(work, "retention")
    write("a.txt", "v1\n"); run("add", "a.txt"); run("commit", "one")
    r1 = latest()
    write("a.txt", "v2\n"); run("commit", "two"); r2 = latest()
    write("a.txt", "v3\n"); run("commit", "three"); r3 = latest()
    expect_out("default retention is 30 days", "Retention: 30 days", "retention")
    expect_out("set retention", "Retention set to 7 days", "retention", "set", "7")
    expect_fail("set rejects non-numbers", "retention", "set", "abc")
    expect_fail("set needs a value", "retention", "set")
    expect_out("recent commits are kept", "keep (newest)", "retention", "status")
    expect_no_out("nothing expired yet", "EXPIRED", "retention", "status")
    rewrite_commit_time(r1)
    rewrite_commit_time(r2)
    write(".minivcs/branches/feature", r2 + "\n")   # a branch tip protects r2
    expect_out("old commit marked expired", "EXPIRED", "retention", "status")
    expect_out("branch tip is protected", "keep (branch tip)", "retention", "status")
    expect_out("dry run reports 1 candidate",
               "1 commit snapshot(s) would be removed",
               "retention", "cleanup", "--dry-run")
    check("dry run removes nothing", (CWD / f".minivcs/objects/{r1}/files").is_dir())
    expect_out("cleanup removes 1 snapshot", "1 commit snapshot(s) removed",
               "retention", "cleanup")
    check("pruned snapshot files gone", not (CWD / f".minivcs/objects/{r1}/files").exists())
    check("pruned commit keeps commit.txt", (CWD / f".minivcs/objects/{r1}/commit.txt").is_file())
    check("pruned marker written", (CWD / f".minivcs/objects/{r1}/pruned").is_file())
    check("protected tip keeps its files", (CWD / f".minivcs/objects/{r2}/files").is_dir())
    check("newest commit keeps its files", (CWD / f".minivcs/objects/{r3}/files").is_dir())
    expect_out("status shows pruned", "pruned (history kept)", "retention", "status")
    expect_out("history still has all commits", f"Commit: {r1}", "history")
    expect_out("parent chain survives", f"Parent: {r1}", "history")
    expect_out("show reports pruned snapshot", "removed by retention policy", "show", r1)
    expect_out("restore of pruned is refused", "retention policy", "restore", r1)
    expect_out("diff against pruned is refused", "retention policy", "diff", r1)
    expect_ok("restore of kept commit works", "restore", r3)
    write("a.txt", "v4\n")
    expect_ok("commit after pruning works", "commit", "four")
    expect_out("cleanup is logged", r1, "retention", "log")
    expect_out("second cleanup does nothing", "0 commit snapshot(s) removed",
               "retention", "cleanup")

    print("== Windows line endings (CRLF) ==")
    fresh_repo(work, "crlf")
    write("w.txt", "one\r\ntwo\r\nthree\r\n"); run("add", "w.txt"); run("commit", "crlf one")
    w1 = latest()
    write("w.txt", "one\r\nTWO\r\nthree\r\n")
    expect_out("CRLF edit detected", "Modified: w.txt", "status")
    _, crlf_diff = run("diff")
    check("CRLF diff shows the change", "+TWO" in crlf_diff and "-two" in crlf_diff, crlf_diff)
    check("CRLF diff has no stray carriage returns", "\r" not in crlf_diff.replace("\r\n", "\n"), repr(crlf_diff))
    run("commit", "crlf two")
    expect_ok("CRLF file restores byte-for-byte", "restore", w1, "--force")
    check("CRLF content preserved", read("w.txt") == "one\r\ntwo\r\nthree\r\n")

    print("== Repository hygiene ==")
    for stray in ("hash-test.txt", "newfile.txt", "test.txt"):
        check(f"stray {stray} removed", not (ROOT / stray).exists())

    print("== 8-12. Not implemented yet ==")
    skip("8.  create/list/switch/delete branches (no branch code)")
    skip("9.  branch pointers persist after restart")
    skip("10. commit updates the correct branch pointer (only 'main' exists)")
    skip("11. merge and conflicts")
    skip("12. SQLite persistence and authentication")


if __name__ == "__main__":
    sys.exit(main())
