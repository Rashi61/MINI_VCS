import os
import shutil
import tempfile
import unittest
from pathlib import Path
import uuid
import io

TEST_ROOT = Path(
    tempfile.mkdtemp(
        prefix="minivcs-api-test-"
    )
)

TEST_DATABASE = TEST_ROOT / "api-test.db"
TEST_REPOSITORIES = TEST_ROOT / "repositories"
PROJECT_ROOT = Path(__file__).resolve().parents[2]
TEST_EXECUTABLE = (
    PROJECT_ROOT / "build" / "Debug" / "minivcs.exe"
)

os.environ["MINIVCS_DATABASE_PATH"] = str(
    TEST_DATABASE
)
os.environ["MINIVCS_REPOSITORY_ROOT"] = str(
    TEST_REPOSITORIES
)
os.environ["MINIVCS_EXECUTABLE"] = str(
    TEST_EXECUTABLE
)

from server.app.api import create_app


class MiniVCSApiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = create_app()
        cls.app.config["TESTING"] = True
        cls.client = cls.app.test_client()

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(
            TEST_ROOT,
            ignore_errors=True,
        )

        os.environ.pop(
            "MINIVCS_DATABASE_PATH",
            None,
        )
        os.environ.pop(
            "MINIVCS_REPOSITORY_ROOT",
            None,
        )
        os.environ.pop(
            "MINIVCS_EXECUTABLE",
            None,
        )

    def setUp(self):
        unique_id = uuid.uuid4().hex[:8]

        self.username = (
            f"api_test_user_{unique_id}"
        )

        self.email = (
            f"api_test_user_{unique_id}@example.com"
        )

        self.password = "TestPassword123"

    def register_and_login(self):
        register_response = self.client.post(
            "/api/auth/register",
            json={
                "username": self.username,
                "email": self.email,
                "password": self.password,
            },
        )

        self.assertEqual(
            register_response.status_code,
            201,
        )

        login_response = self.client.post(
            "/api/auth/login",
            json={
                "username": self.username,
                "password": self.password,
            },
        )

        self.assertEqual(
            login_response.status_code,
            200,
        )

        body = login_response.get_json()

        self.assertTrue(body["ok"])
        self.assertIn("token", body["data"])

        return body["data"]["token"]

    def test_health(self):
        response = self.client.get(
            "/api/health"
        )

        self.assertEqual(
            response.status_code,
            200,
        )

        body = response.get_json()

        self.assertTrue(body["ok"])
        self.assertEqual(
            body["data"]["status"],
            "healthy",
        )
        self.assertEqual(
            body["data"]["database"],
            "ready",
        )

    def test_registration_and_login(self):
        token = self.register_and_login()

        self.assertTrue(
            isinstance(token, str)
        )
        self.assertGreater(
            len(token),
            10,
        )

    def test_repository_creation_requires_login(self):
        response = self.client.post(
            "/api/repositories",
            json={
                "name": "unauthorized",
                "visibility": "public",
            },
        )

        self.assertEqual(
            response.status_code,
            401,
        )

        body = response.get_json()

        self.assertFalse(body["ok"])
        self.assertEqual(
            body["error_code"],
            "AUTHENTICATION_REQUIRED",
        )

    def test_create_private_repository(self):
        token = self.register_and_login()

        response = self.client.post(
            "/api/repositories",
            headers={
                "Authorization": f"Bearer {token}",
            },
            json={
                "name": "private-demo",
                "visibility": "private",
            },
        )

        self.assertEqual(
            response.status_code,
            201,
        )

        body = response.get_json()

        self.assertTrue(body["ok"])
        self.assertEqual(
            body["data"]["name"],
            "private-demo",
        )
        self.assertEqual(
            body["data"]["visibility"],
            "private",
        )

        self.assertNotIn(
            "path",
            body["data"],
        )

    def test_public_listing_hides_private_repository(self):
        token = self.register_and_login()

        create_response = self.client.post(
            "/api/repositories",
            headers={
                "Authorization": f"Bearer {token}",
            },
            json={
                "name": "private-hidden",
                "visibility": "private",
            },
        )

        self.assertEqual(
            create_response.status_code,
            201,
        )

        public_response = self.client.get(
            "/api/repositories"
        )

        self.assertEqual(
            public_response.status_code,
            200,
        )

        repositories = (
            public_response
            .get_json()["data"]["repositories"]
        )

        names = {
            repository["name"]
            for repository in repositories
        }

        self.assertNotIn(
            "private-hidden",
            names,
        )

    def test_public_repository_is_listed(self):
        token = self.register_and_login()

        create_response = self.client.post(
            "/api/repositories",
            headers={
                "Authorization": f"Bearer {token}",
            },
            json={
                "name": "public-demo",
                "visibility": "public",
            },
        )

        self.assertEqual(
            create_response.status_code,
            201,
        )

        public_response = self.client.get(
            "/api/repositories"
        )

        repositories = (
            public_response
            .get_json()["data"]["repositories"]
        )

        names = {
            repository["name"]
            for repository in repositories
        }

        self.assertIn(
            "public-demo",
            names,
        )

    def test_logout_invalidates_token(self):
        token = self.register_and_login()

        logout_response = self.client.post(
            "/api/auth/logout",
            headers={
                "Authorization": f"Bearer {token}",
            },
        )

        self.assertEqual(
            logout_response.status_code,
            200,
        )

        create_response = self.client.post(
            "/api/repositories",
            headers={
                "Authorization": f"Bearer {token}",
            },
            json={
                "name": "after-logout",
                "visibility": "public",
            },
        )

        self.assertEqual(
            create_response.status_code,
            401,
        )
    def test_private_repository_requires_access(self):
        token = self.register_and_login()

        response = self.client.post(
            "/api/repositories",
            headers={
                "Authorization": f"Bearer {token}",
            },
            json={
                "name": "private-details",
                "visibility": "private",
            },
        )

        self.assertEqual(
            response.status_code,
            201,
        )

        repository_id = (
            response
            .get_json()["data"]["id"]
        )

        details = self.client.get(
            f"/api/repositories/{repository_id}"
        )

        self.assertEqual(
            details.status_code,
            403,
        )

        authorized_details = self.client.get(
            f"/api/repositories/{repository_id}",
            headers={
                "Authorization": f"Bearer {token}",
            },
        )

        self.assertEqual(
            authorized_details.status_code,
            200,
        )
    def test_public_repository_details_are_visible(self):
        token = self.register_and_login()

        response = self.client.post(
            "/api/repositories",
            headers={
                "Authorization": f"Bearer {token}",
            },
            json={
                "name": "public-details",
                "visibility": "public",
            },
        )

        self.assertEqual(
            response.status_code,
            201,
        )

        repository_id = (
            response
            .get_json()["data"]["id"]
        )

        details = self.client.get(
            f"/api/repositories/{repository_id}"
        )

        self.assertEqual(
            details.status_code,
            200,
        )

        body = details.get_json()

        self.assertTrue(body["ok"])
        self.assertEqual(
            body["data"]["name"],
            "public-details",
        )
    def test_list_branches_for_public_repository(self):
        token = self.register_and_login()

        create_response = self.client.post(
            "/api/repositories",
            headers={
                "Authorization": f"Bearer {token}",
            },
            json={
                "name": "branch-list-demo",
                "visibility": "public",
            },
        )

        self.assertEqual(
            create_response.status_code,
            201,
        )

        repository_id = (
            create_response
            .get_json()["data"]["id"]
        )

        branches_response = self.client.get(
            f"/api/repositories/{repository_id}/branches"
        )

        self.assertEqual(
            branches_response.status_code,
            200,
        )

        body = branches_response.get_json()

        self.assertTrue(body["ok"])
        self.assertIn(
            "main",
            body["data"]["output"],
        )

    def test_create_branch_requires_permission(self):
        token = self.register_and_login()

        create_response = self.client.post(
            "/api/repositories",
            headers={
                "Authorization": f"Bearer {token}",
            },
            json={
                "name": "branch-create-demo",
                "visibility": "public",
            },
        )

        self.assertEqual(
            create_response.status_code,
            201,
        )

        repository_id = (
            create_response
            .get_json()["data"]["id"]
        )

        branch_response = self.client.post(
            f"/api/repositories/{repository_id}/branches",
            headers={
                "Authorization": f"Bearer {token}",
            },
            json={
                "name": "feature-api",
            },
        )

        self.assertEqual(
            branch_response.status_code,
            201,
        )

        self.assertIn(
            "feature-api",
            branch_response
            .get_json()["data"]["output"],
        )

    def test_commit_requires_message(self):
        token = self.register_and_login()

        create_response = self.client.post(
            "/api/repositories",
            headers={
                "Authorization": f"Bearer {token}",
            },
            json={
                "name": "commit-validation-demo",
                "visibility": "private",
            },
        )

        self.assertEqual(
            create_response.status_code,
            201,
        )

        repository_id = (
            create_response
            .get_json()["data"]["id"]
        )

        commit_response = self.client.post(
            f"/api/repositories/{repository_id}/commit",
            headers={
                "Authorization": f"Bearer {token}",
            },
            json={
                "message": "",
            },
        )

        self.assertEqual(
            commit_response.status_code,
            400,
        )

        body = commit_response.get_json()

        self.assertFalse(body["ok"])
        self.assertEqual(
            body["error_code"],
            "VALIDATION_ERROR",
        )
    def test_upload_file_requires_login(self):
        response = self.client.post(
            "/api/repositories/1/files",
            data={
                "path": "README.md",
                "file": (
                    io.BytesIO(b"unauthorized"),
                    "README.md",
                ),
            },
            content_type="multipart/form-data",
        )

        self.assertEqual(
            response.status_code,
            401,
        )

    def test_upload_file_and_stage(self):
        token = self.register_and_login()

        create_response = self.client.post(
            "/api/repositories",
            headers={
                "Authorization": f"Bearer {token}",
            },
            json={
                "name": "upload-demo",
                "visibility": "private",
            },
        )

        self.assertEqual(
            create_response.status_code,
            201,
        )

        repository_id = (
            create_response
            .get_json()["data"]["id"]
        )

        upload_response = self.client.post(
            f"/api/repositories/{repository_id}/files",
            headers={
                "Authorization": f"Bearer {token}",
            },
            data={
                "path": "docs/readme.txt",
                "file": (
                    io.BytesIO(b"MiniVCS upload test"),
                    "readme.txt",
                ),
            },
            content_type="multipart/form-data",
        )

        self.assertEqual(
            upload_response.status_code,
            201,
        )

        body = upload_response.get_json()

        self.assertTrue(body["ok"])
        self.assertEqual(
            body["data"]["path"],
            "docs/readme.txt",
        )

    def test_upload_rejects_parent_path(self):
        token = self.register_and_login()

        create_response = self.client.post(
            "/api/repositories",
            headers={
                "Authorization": f"Bearer {token}",
            },
            json={
                "name": "upload-safety-demo",
                "visibility": "private",
            },
        )

        repository_id = (
            create_response
            .get_json()["data"]["id"]
        )

        upload_response = self.client.post(
            f"/api/repositories/{repository_id}/files",
            headers={
                "Authorization": f"Bearer {token}",
            },
            data={
                "path": "../outside.txt",
                "file": (
                    io.BytesIO(b"must be rejected"),
                    "outside.txt",
                ),
            },
            content_type="multipart/form-data",
        )

        self.assertEqual(
            upload_response.status_code,
            400,
        )

        body = upload_response.get_json()

        self.assertFalse(body["ok"])
        self.assertEqual(
            body["error_code"],
            "FILE_UPLOAD_ERROR",
        )
    def test_switch_branch_requires_permission(self):
        token = self.register_and_login()

        create_response = self.client.post(
            "/api/repositories",
            headers={
                "Authorization": f"Bearer {token}",
            },
            json={
                "name": "branch-switch-demo",
                "visibility": "private",
            },
        )

        self.assertEqual(
            create_response.status_code,
            201,
        )

        repository_id = (
            create_response
            .get_json()["data"]["id"]
        )

        create_branch_response = (
            self.client.post(
                f"/api/repositories/{repository_id}/branches",
                headers={
                    "Authorization": f"Bearer {token}",
                },
                json={
                    "name": "feature-switch",
                },
            )
        )

        self.assertEqual(
            create_branch_response.status_code,
            201,
        )

        switch_response = self.client.post(
            f"/api/repositories/{repository_id}/branches/switch",
            headers={
                "Authorization": f"Bearer {token}",
            },
            json={
                "name": "feature-switch",
            },
        )

        self.assertEqual(
            switch_response.status_code,
            200,
        )

        self.assertTrue(
            switch_response
            .get_json()["ok"]
        )
    def test_remote_push_and_pull(self):
        token = self.register_and_login()

        create_response = self.client.post(
            "/api/repositories",
            headers={
                "Authorization": f"Bearer {token}",
            },
            json={
                "name": "remote-api-demo",
                "visibility": "private",
            },
        )

        self.assertEqual(
            create_response.status_code,
            201,
        )

        repository_id = (
            create_response
            .get_json()["data"]["id"]
        )

        upload_response = self.client.post(
            f"/api/repositories/{repository_id}/files",
            headers={
                "Authorization": f"Bearer {token}",
            },
            data={
                "path": "remote-file.txt",
                "file": (
                    io.BytesIO(b"remote API test"),
                    "remote-file.txt",
                ),
            },
            content_type="multipart/form-data",
        )

        self.assertEqual(
            upload_response.status_code,
            201,
        )

        push_response = self.client.post(
            f"/api/repositories/{repository_id}/remote/push",
            headers={
                "Authorization": f"Bearer {token}",
            },
        )

        self.assertEqual(
            push_response.status_code,
            200,
        )

        body = push_response.get_json()

        self.assertTrue(body["ok"])
        self.assertEqual(
            body["data"]["operation"],
            "push",
        )
        
if __name__ == "__main__":
    unittest.main()