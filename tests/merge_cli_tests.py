"""End-to-end tests for `minivcs merge`.

Usage: python tests/merge_cli_tests.py <path-to-minivcs-executable>
"""
import os
import shutil
import subprocess
import sys
import tempfile

EXE = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else None
passed = failed = 0


def run(cwd, *args):
    return subprocess.run(
        [EXE, *args], cwd=cwd, capture_output=True, text=True, timeout=60
    )


def write(root, name, text):
    with open(os.path.join(root, name), "w", newline="\n") as handle:
        handle.write(text)


def read(root, name):
    with open(os.path.join(root, name), newline="") as handle:
        return handle.read().replace("\r\n", "\n")


def check(name, condition, detail=""):
    global passed, failed
    if condition:
        passed += 1
        print(f"  PASS  {name}")
    else:
        failed += 1
        print(f"  FAIL  {name}  {detail}")


def new_repo():
    root = tempfile.mkdtemp(prefix="minivcs-merge-cli-")
    assert run(root, "init", root).returncode == 0
    return root


def track_and_commit(root, message, *files):
    for name in files:
        assert run(root, "add", name).returncode == 0, name
    result = run(root, "commit", message)
    assert result.returncode == 0, result.stderr + result.stdout
    return result


def test_commit_message_printed_once():
    root = new_repo()
    write(root, "a.txt", "x\n")
    result = track_and_commit(root, "first", "a.txt")
    check(
        "commit success message printed once",
        result.stdout.count("Commit created successfully") == 1,
        result.stdout,
    )
    shutil.rmtree(root, ignore_errors=True)


def test_clean_merge_updates_working_tree():
    root = new_repo()
    write(root, "keep.txt", "a\n")
    write(root, "gone.txt", "will be deleted\n")
    track_and_commit(root, "base", "keep.txt", "gone.txt")

    assert run(root, "branch", "create", "feature").returncode == 0
    assert run(root, "switch", "feature").returncode == 0
    write(root, "keep.txt", "a\nfrom feature\n")
    write(root, "new.txt", "new file\n")
    assert run(root, "add", "keep.txt").returncode == 0
    assert run(root, "add", "new.txt").returncode == 0
    assert run(root, "remove", "gone.txt").returncode == 0
    if os.path.exists(os.path.join(root, "gone.txt")):
        os.remove(os.path.join(root, "gone.txt"))
    assert run(root, "commit", "feature work").returncode == 0
    assert run(root, "switch", "main").returncode == 0

    merged = run(root, "merge", "feature")
    check("clean merge exits 0", merged.returncode == 0, merged.stderr + merged.stdout)
    check("merged content reaches working tree",
          read(root, "keep.txt") == "a\nfrom feature\n", read(root, "keep.txt"))
    check("file added on other branch exists",
          os.path.exists(os.path.join(root, "new.txt")))
    check("file deleted on other branch is removed",
          not os.path.exists(os.path.join(root, "gone.txt")))
    check("repository verifies after merge",
          run(root, "verify").returncode == 0, run(root, "verify").stdout)
    check("can switch branches after merge",
          run(root, "switch", "feature").returncode == 0
          and run(root, "switch", "main").returncode == 0)
    check("history records the merge commit",
          "Merge branch 'feature'" in run(root, "history").stdout)
    shutil.rmtree(root, ignore_errors=True)


def test_conflict_writes_markers_and_fails():
    root = new_repo()
    write(root, "c.txt", "base\n")
    track_and_commit(root, "base", "c.txt")
    assert run(root, "branch", "create", "other").returncode == 0
    assert run(root, "switch", "other").returncode == 0
    write(root, "c.txt", "other side\n")
    track_and_commit(root, "other", "c.txt")
    assert run(root, "switch", "main").returncode == 0
    write(root, "c.txt", "main side\n")
    track_and_commit(root, "main", "c.txt")

    merged = run(root, "merge", "other")
    check("conflicting merge exits non-zero", merged.returncode != 0)
    check("conflict is reported by file name", "CONFLICT: c.txt" in merged.stderr, merged.stderr)
    content = read(root, "c.txt")
    check("conflict markers written",
          "<<<<<<< ours" in content and "=======" in content
          and ">>>>>>> theirs" in content, content)
    shutil.rmtree(root, ignore_errors=True)


def test_local_edits_are_protected():
    root = new_repo()
    write(root, "p.txt", "base\n")
    track_and_commit(root, "base", "p.txt")
    assert run(root, "branch", "create", "side").returncode == 0
    assert run(root, "switch", "side").returncode == 0
    write(root, "p.txt", "side change\n")
    track_and_commit(root, "side", "p.txt")
    assert run(root, "switch", "main").returncode == 0

    write(root, "p.txt", "my uncommitted edit\n")
    merged = run(root, "merge", "side")
    check("merge refuses to overwrite uncommitted edits",
          merged.returncode != 0 and "Local changes" in merged.stderr,
          merged.stderr + merged.stdout)
    check("uncommitted edit is untouched",
          read(root, "p.txt") == "my uncommitted edit\n")
    shutil.rmtree(root, ignore_errors=True)


def test_legacy_commit_format_is_readable():
    import glob

    root = new_repo()
    write(root, "a.txt", "1\n")
    track_and_commit(root, "one", "a.txt")
    write(root, "b.txt", "2\n")
    track_and_commit(root, "two", "b.txt")

    # Rewrite commits into the older format written by early MiniVCS builds:
    # id, message, date, parent, count, files  (no author / second parent).
    for path in glob.glob(os.path.join(root, ".minivcs", "objects", "*", "commit.txt")):
        with open(path) as handle:
            lines = handle.read().split("\n")
        legacy = lines[0:3] + [lines[4]] + lines[6:]
        with open(path, "w", newline="\n") as handle:
            handle.write("\n".join(legacy))

    history = run(root, "history")
    check("legacy-format history is readable",
          history.returncode == 0 and "Message: one" in history.stdout
          and "Message: two" in history.stdout, history.stderr + history.stdout)
    write(root, "c.txt", "3\n")
    add = run(root, "add", "c.txt")
    commit = run(root, "commit", "three")
    check("new commit works on top of legacy history",
          add.returncode == 0 and commit.returncode == 0, commit.stderr)
    shutil.rmtree(root, ignore_errors=True)


def test_bad_arguments():
    root = new_repo()
    write(root, "a.txt", "x\n")
    track_and_commit(root, "first", "a.txt")
    check("merge without branch name fails", run(root, "merge").returncode != 0)
    check("merge of unknown branch fails", run(root, "merge", "nope").returncode != 0)
    shutil.rmtree(root, ignore_errors=True)


if __name__ == "__main__":
    if not EXE or not os.path.isfile(EXE):
        print("Usage: python tests/merge_cli_tests.py <minivcs executable>")
        sys.exit(2)

    for test in (
        test_commit_message_printed_once,
        test_clean_merge_updates_working_tree,
        test_conflict_writes_markers_and_fails,
        test_local_edits_are_protected,
        test_legacy_commit_format_is_readable,
        test_bad_arguments,
    ):
        print(test.__name__)
        test()

    print(f"\n{passed} passed, {failed} failed")
    sys.exit(1 if failed else 0)
