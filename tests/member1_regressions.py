import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def run(exe, args, cwd):
    result = subprocess.run(
        [str(exe), *args],
        cwd=str(cwd),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    return result.returncode, result.stdout + result.stderr


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def main():
    if len(sys.argv) != 2:
        print("Usage: python tests/member1_regressions.py <minivcs-exe>")
        return 2

    exe = Path(sys.argv[1]).resolve()
    check(exe.is_file(), f"Executable not found: {exe}")

    temp_root = Path(tempfile.mkdtemp(prefix="minivcs-member1-"))

    try:
        repo = temp_root / "repo"
        nested = repo / "src" / "module"
        nested.mkdir(parents=True)

        code, output = run(exe, ["init"], repo)
        check(code == 0, f"init failed:\n{output}")

        (repo / "file.txt").write_text(
            "version one\n",
            encoding="utf-8",
        )

        code, output = run(exe, ["add", "file.txt"], repo)
        check(code == 0, f"add failed:\n{output}")

        code, output = run(
            exe,
            ["commit", "first commit"],
            repo,
        )
        check(code == 0, f"commit failed:\n{output}")

        print("PASS: repository initialized and first commit created")

        # 1. Nested repository discovery.
        code, output = run(exe, ["history"], nested)
        check(
            code == 0 and "first commit" in output,
            f"nested history failed:\n{output}",
        )
        print("PASS: commands work from nested directory")

        # 2. Ignore rules must apply to add.
        (repo / ".minivcsignore").write_text(
            "ignored/\n",
            encoding="utf-8",
        )

        ignored_dir = repo / "ignored"
        ignored_dir.mkdir()
        (ignored_dir / "generated.txt").write_text(
            "generated\n",
            encoding="utf-8",
        )

        code, output = run(
            exe,
            ["add", "ignored/generated.txt"],
            repo,
        )
        check(
            code != 0 and "ignored" in output.lower(),
            f"ignored file was accepted:\n{output}",
        )
        print("PASS: add rejects .minivcsignore paths")

        # 3. Corrupt branch pointer must not appear as empty history.
        branch_file = repo / ".minivcs" / "branches" / "main"
        original_branch = branch_file.read_text(
            encoding="utf-8",
        )

        branch_file.write_text(
            "missing-commit-id\n",
            encoding="utf-8",
        )

        code, output = run(exe, ["history"], repo)
        check(
            code != 0 and
            "corrupt" in output.lower() and
            "no commits yet" not in output.lower(),
            f"corruption was not reported correctly:\n{output}",
        )
        print("PASS: corrupt branch history is reported")

        branch_file.write_text(
            original_branch,
            encoding="utf-8",
        )

        # 4. Checkout/restore must not move branch pointer.
        (repo / "file.txt").write_text(
            "version two\n",
            encoding="utf-8",
        )

        code, output = run(
            exe,
            ["commit", "second commit"],
            repo,
        )
        check(code == 0, f"second commit failed:\n{output}")

        second_commit = branch_file.read_text(
            encoding="utf-8",
        ).strip()

        first_commit = None
        for line in output.splitlines():
            if "Commit ID:" in line:
                first_commit = line.split(":", 1)[1].strip()

        # The first commit ID is found through history if command output
        # format changes.
        code, history = run(exe, ["history"], repo)
        check(code == 0, f"history failed:\n{history}")

        commit_lines = [
            line.split(":", 1)[1].strip()
            for line in history.splitlines()
            if line.startswith("Commit:")
        ]

        check(
            len(commit_lines) >= 2,
            f"Could not find two commits:\n{history}",
        )

        oldest_commit = commit_lines[-1]

        code, output = run(
            exe,
            ["restore", oldest_commit, "--force"],
            repo,
        )
        check(code == 0, f"restore failed:\n{output}")

        current_branch = branch_file.read_text(
            encoding="utf-8",
        ).strip()

        check(
            current_branch == second_commit,
            "restore moved the branch pointer unexpectedly",
        )

        check(
            (repo / "file.txt").read_text(
                encoding="utf-8",
            ) == "version one\n",
            "restore did not restore the first snapshot",
        )

        print("PASS: restore changes files without moving branch pointer")
        print("All Member 1 regression tests passed")
        return 0

    finally:
        shutil.rmtree(temp_root, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
