"""Tests for merge, retention, structured history, rate limiting and
frontend serving. Run from the project root:

    python -m unittest server.tests.test_extras -v
"""
import io
import os
import shutil
import tempfile
import unittest
import uuid
from pathlib import Path

TEST_ROOT = Path(tempfile.mkdtemp(prefix="minivcs-extras-test-"))

from server.app.config import load_settings  # noqa: E402

# Resolve the executable once, at import time, before any other test module
# can change the environment.
EXECUTABLE = os.environ.get("MINIVCS_EXECUTABLE") or str(
    load_settings().minivcs_executable
)

TEST_ENV = {
    "MINIVCS_EXECUTABLE": EXECUTABLE,
    "MINIVCS_DATABASE_PATH": str(TEST_ROOT / "extras.db"),
    "MINIVCS_REPOSITORY_ROOT": str(TEST_ROOT / "repositories"),
    "MINIVCS_REMOTE_ROOT": str(TEST_ROOT / "remotes"),
    "MINIVCS_REGISTER_MAX_ATTEMPTS": "100000",
    "MINIVCS_LOGIN_MAX_ATTEMPTS": "3",
    "MINIVCS_LOGIN_WINDOW_SECONDS": "300",
}

from server.app.api import create_app  # noqa: E402
from server.app.repository_service import (  # noqa: E402
    parse_history_output,
    parse_retention_output,
    validate_branch_name,
    validate_retention_days,
    RepositoryServiceError,
)


class ParserTests(unittest.TestCase):
    def test_parse_history(self):
        text = (
            "\nCommit: aaa111\nMessage: second\n"
            "Date: 2026-01-02 10:00:00\nParent: bbb222\n\n"
            "Commit: bbb222\nMessage: first\n"
            "Date: 2026-01-01 10:00:00\nParent: none\n"
        )
        commits = parse_history_output(text)

        self.assertEqual(len(commits), 2)
        self.assertEqual(commits[0]["id"], "aaa111")
        self.assertEqual(commits[0]["parent"], "bbb222")
        self.assertIsNone(commits[1]["parent"])

    def test_parse_retention(self):
        text = (
            "Retention: 30 days\n"
            "  c533209d  2026-11-23 02:45  0d  keep (newest)\n"
            "  075e6fe2  2026-11-23 02:45  4d  expired\n"
        )
        parsed = parse_retention_output(text)

        self.assertEqual(parsed["days"], 30)
        self.assertEqual(len(parsed["snapshots"]), 2)
        self.assertEqual(parsed["snapshots"][1]["age_days"], 4)
        self.assertEqual(parsed["snapshots"][1]["result"], "expired")

    def test_validators(self):
        self.assertEqual(validate_branch_name(" feat-1 "), "feat-1")
        self.assertEqual(validate_retention_days("7"), 7)

        for bad in ["", "../x", "a b", "-x", "a" * 80, 5, None]:
            with self.assertRaises(RepositoryServiceError):
                validate_branch_name(bad)

        for bad in [0, -1, "x", 99999, None]:
            with self.assertRaises(RepositoryServiceError):
                validate_retention_days(bad)


class ExtrasApiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._saved_env = {k: os.environ.get(k) for k in TEST_ENV}
        os.environ.update(TEST_ENV)
        cls.app = create_app()
        cls.app.config["TESTING"] = True
        cls.client = cls.app.test_client()

    @classmethod
    def tearDownClass(cls):
        for key, value in cls._saved_env.items():
            if value is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = value

        shutil.rmtree(TEST_ROOT, ignore_errors=True)

    def make_user(self):
        suffix = uuid.uuid4().hex[:8]
        name = f"extra_{suffix}"
        password = "TestPassword123"

        self.client.post(
            "/api/auth/register",
            json={
                "username": name,
                "email": f"{name}@example.com",
                "password": password,
            },
        )
        response = self.client.post(
            "/api/auth/login",
            json={"username": name, "password": password},
        )
        token = response.get_json()["data"]["token"]
        return name, {"Authorization": f"Bearer {token}"}

    def make_repo(self, headers, visibility="private"):
        response = self.client.post(
            "/api/repositories",
            headers=headers,
            json={
                "name": f"repo-{uuid.uuid4().hex[:6]}",
                "visibility": visibility,
            },
        )
        self.assertEqual(response.status_code, 201)
        return response.get_json()["data"]["id"]

    def upload(self, headers, repo_id, path, content):
        response = self.client.post(
            f"/api/repositories/{repo_id}/files",
            headers=headers,
            data={"path": path, "file": (io.BytesIO(content), "f")},
            content_type="multipart/form-data",
        )
        self.assertEqual(response.status_code, 201, response.get_json())

    def commit(self, headers, repo_id, message):
        response = self.client.post(
            f"/api/repositories/{repo_id}/commit",
            headers=headers,
            json={"message": message},
        )
        self.assertEqual(response.status_code, 201, response.get_json())

    def create_branch(self, headers, repo_id, name):
        response = self.client.post(
            f"/api/repositories/{repo_id}/branches",
            headers=headers,
            json={"name": name},
        )
        self.assertEqual(response.status_code, 201, response.get_json())

    def switch(self, headers, repo_id, name):
        response = self.client.post(
            f"/api/repositories/{repo_id}/branches/switch",
            headers=headers,
            json={"name": name},
        )
        self.assertEqual(response.status_code, 200, response.get_json())

    # ---- frontend + headers -------------------------------------------
    def test_home_page_is_served_with_security_headers(self):
        response = self.client.get("/")

        self.assertEqual(response.status_code, 200)
        self.assertIn(b"MiniVCS", response.data)
        self.assertEqual(
            response.headers["X-Content-Type-Options"], "nosniff"
        )
        self.assertIn(
            "default-src 'self'",
            response.headers["Content-Security-Policy"],
        )

    def test_static_pages_and_unknown_api_path(self):
        for page in [
            "login.html",
            "register.html",
            "history.html",
            "retention.html",
            "repositories.html",
            "public-repositories.html",
        ]:
            self.assertEqual(self.client.get("/" + page).status_code, 200)

        missing = self.client.get("/api/does-not-exist")
        self.assertEqual(missing.status_code, 404)
        self.assertFalse(missing.get_json()["ok"])

    def test_path_traversal_is_blocked(self):
        response = self.client.get("/..%2Fserver%2Fschema.sql")
        self.assertIn(response.status_code, (400, 404))

    # ---- rate limiting -----------------------------------------------
    def test_login_rate_limit(self):
        name = f"limit_{uuid.uuid4().hex[:8]}"
        codes = []

        for _ in range(5):
            codes.append(
                self.client.post(
                    "/api/auth/login",
                    json={"username": name, "password": "wrong-password"},
                ).status_code
            )

        self.assertEqual(codes[:3], [401, 401, 401])
        self.assertEqual(codes[3:], [429, 429])

    # ---- history / retention -----------------------------------------
    def test_structured_commits_and_retention(self):
        _, headers = self.make_user()
        repo_id = self.make_repo(headers)

        self.upload(headers, repo_id, "a.txt", b"one\n")
        self.commit(headers, repo_id, "first commit")

        response = self.client.get(
            f"/api/repositories/{repo_id}/commits", headers=headers
        )
        self.assertEqual(response.status_code, 200)
        commits = response.get_json()["data"]["commits"]
        self.assertEqual(len(commits), 1)
        self.assertEqual(commits[0]["message"], "first commit")

        status = self.client.get(
            f"/api/repositories/{repo_id}/retention", headers=headers
        )
        self.assertEqual(status.status_code, 200)
        self.assertEqual(status.get_json()["data"]["days"], 30)

        saved = self.client.post(
            f"/api/repositories/{repo_id}/retention",
            headers=headers,
            json={"days": 5},
        )
        self.assertEqual(saved.status_code, 200, saved.get_json())
        self.assertEqual(saved.get_json()["data"]["days"], 5)

        bad = self.client.post(
            f"/api/repositories/{repo_id}/retention",
            headers=headers,
            json={"days": -3},
        )
        self.assertEqual(bad.status_code, 400)

        cleanup = self.client.post(
            f"/api/repositories/{repo_id}/retention/cleanup",
            headers=headers,
        )
        self.assertEqual(cleanup.status_code, 200, cleanup.get_json())

    def test_retention_changes_require_login_and_permission(self):
        _, owner = self.make_user()
        repo_id = self.make_repo(owner, visibility="public")

        anonymous = self.client.post(
            f"/api/repositories/{repo_id}/retention", json={"days": 3}
        )
        self.assertEqual(anonymous.status_code, 401)

        _, stranger = self.make_user()
        denied = self.client.post(
            f"/api/repositories/{repo_id}/retention/cleanup",
            headers=stranger,
        )
        self.assertEqual(denied.status_code, 403)

    # ---- merge ---------------------------------------------------------
    def test_clean_merge(self):
        _, headers = self.make_user()
        repo_id = self.make_repo(headers)

        self.upload(headers, repo_id, "a.txt", b"base\n")
        self.commit(headers, repo_id, "base")
        self.create_branch(headers, repo_id, "feature")
        self.switch(headers, repo_id, "feature")
        self.upload(headers, repo_id, "b.txt", b"feature file\n")
        self.commit(headers, repo_id, "feature work")
        self.switch(headers, repo_id, "main")

        merged = self.client.post(
            f"/api/repositories/{repo_id}/merge",
            headers=headers,
            json={"source": "feature"},
        )
        self.assertEqual(merged.status_code, 200, merged.get_json())

        commits = self.client.get(
            f"/api/repositories/{repo_id}/commits", headers=headers
        ).get_json()["data"]["commits"]
        self.assertEqual(commits[0]["message"], "Merge branch 'feature'")

    def test_merge_conflict_returns_409(self):
        _, headers = self.make_user()
        repo_id = self.make_repo(headers)

        self.upload(headers, repo_id, "a.txt", b"base\n")
        self.commit(headers, repo_id, "base")
        self.create_branch(headers, repo_id, "other")
        self.switch(headers, repo_id, "other")
        self.upload(headers, repo_id, "a.txt", b"from other\n")
        self.commit(headers, repo_id, "other change")
        self.switch(headers, repo_id, "main")
        self.upload(headers, repo_id, "a.txt", b"from main\n")
        self.commit(headers, repo_id, "main change")

        merged = self.client.post(
            f"/api/repositories/{repo_id}/merge",
            headers=headers,
            json={"source": "other"},
        )
        self.assertEqual(merged.status_code, 409, merged.get_json())
        self.assertIn("CONFLICT", merged.get_json()["message"])

    def test_merge_validation_and_auth(self):
        _, headers = self.make_user()
        repo_id = self.make_repo(headers)

        anonymous = self.client.post(
            f"/api/repositories/{repo_id}/merge", json={"source": "x"}
        )
        self.assertEqual(anonymous.status_code, 401)

        evil = self.client.post(
            f"/api/repositories/{repo_id}/merge",
            headers=headers,
            json={"source": "../../etc/passwd"},
        )
        self.assertEqual(evil.status_code, 400)

        missing = self.client.post(
            f"/api/repositories/{repo_id}/merge",
            headers=headers,
            json={"source": "does-not-exist"},
        )
        self.assertIn(missing.status_code, (400, 404))


if __name__ == "__main__":
    unittest.main()
