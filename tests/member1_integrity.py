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
        print("Usage: python tests/member1_integrity.py <minivcs-exe>")
        return 2

    exe = Path(sys.argv[1]).resolve()
    check(exe.is_file(), f"Executable not found: {exe}")
    temp_root = Path(tempfile.mkdtemp(prefix="minivcs-integrity-"))

    try:
        repo = temp_root / "repo"
        repo.mkdir()

        code, output = run(exe, ["init"], repo)
        check(code == 0, f"init failed:\n{output}")

        (repo / "file.txt").write_text(
            "hello\n",
            encoding="utf-8",
        )

        code, output = run(exe, ["add", "file.txt"], repo)
        check(code == 0, f"add failed:\n{output}")

        code, output = run(
            exe,
            ["commit", "integrity test"],
            repo,
        )
        check(code == 0, f"commit failed:\n{output}")

        code, output = run(exe, ["verify"], repo)
        check(
            code == 0 and "Repository is valid" in output,
            f"valid repository was rejected:\n{output}",
        )
        print("PASS: valid repository is accepted")

        branch_file = repo / ".minivcs" / "branches" / "main"
        original = branch_file.read_text(encoding="utf-8")
        branch_file.write_text(
            "missing-commit-id\n",
            encoding="utf-8",
        )

        code, output = run(exe, ["verify"], repo)
        check(
            code != 0 and "corrupt" in output.lower(),
            f"invalid branch pointer was not detected:\n{output}",
        )
        print("PASS: invalid branch pointer is detected")

        branch_file.write_text(original, encoding="utf-8")
        return 0

    finally:
        shutil.rmtree(temp_root, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())