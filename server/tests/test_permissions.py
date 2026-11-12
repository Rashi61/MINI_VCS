import unittest
import tempfile
from pathlib import Path
from unittest.mock import patch

from server.app.database import initialize_database, get_connection
from server.app.permissions import (
    has_permission,
    get_repository_role,
    check_repository_permission
)


class TestPermissions(unittest.TestCase):

    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.db_path = Path(self.temp_dir.name) / "test.db"

        initialize_database(self.db_path)

        self.patcher = patch(
            "server.app.permissions.get_connection",
            side_effect=lambda: get_connection(self.db_path)
        )
        self.patcher.start()

        conn = get_connection(self.db_path)

        cursor = conn.execute(
            """INSERT INTO users
            (username, email, password_hash, password_salt)
            VALUES (?, ?, ?, ?)""",
            ("rashi", "rashi@test.com", "hash", "salt")
        )

        self.user_id = cursor.lastrowid

        cursor = conn.execute(
            """INSERT INTO repositories (name, owner_id)
            VALUES (?, ?)""",
            ("MiniVCS", self.user_id)
        )

        self.repo_id = cursor.lastrowid
        conn.commit()
        conn.close()

    def tearDown(self):
        self.patcher.stop()
        self.temp_dir.cleanup()

    def test_owner_permissions(self):
        self.assertTrue(has_permission("owner", "delete_repository"))

    def test_maintainer_permissions(self):
        self.assertTrue(has_permission("maintainer", "manage_members"))
        self.assertFalse(has_permission("maintainer", "delete_repository"))

    def test_contributor_permissions(self):
        self.assertTrue(has_permission("contributor", "push"))
        self.assertFalse(has_permission("contributor", "manage_members"))

    def test_viewer_permissions(self):
        self.assertTrue(has_permission("viewer", "view"))
        self.assertFalse(has_permission("viewer", "push"))

    def test_invalid_role(self):
        self.assertFalse(has_permission("guest", "view"))

    def test_database_permission(self):
        self.assertEqual(
            get_repository_role(self.user_id, self.repo_id),
            "owner"
        )

        self.assertTrue(
            check_repository_permission(
                self.user_id, self.repo_id, "push"
            )
        )


if __name__ == "__main__":
    unittest.main()