import unittest
import tempfile
from pathlib import Path
from unittest.mock import patch

from server.app.database import initialize_database, get_connection
from server.app.auth import (
    register_user,
    login_user,
    verify_session,
    logout_user
)


class TestAuthentication(unittest.TestCase):

    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.db_path = Path(self.temp_dir.name) / "test.db"

        initialize_database(self.db_path)

        self.patcher = patch(
            "server.app.auth.get_connection",
            side_effect=lambda: get_connection(self.db_path)
        )
        self.patcher.start()

    def tearDown(self):
        self.patcher.stop()
        self.temp_dir.cleanup()

    def test_registration(self):
        user_id = register_user(
            "rashi", "rashi@test.com", "password123"
        )
        self.assertIsNotNone(user_id)

    def test_duplicate_registration(self):
        register_user("rashi", "rashi@test.com", "password123")

        with self.assertRaises(ValueError):
            register_user("rashi", "other@test.com", "password123")

    def test_login(self):
        register_user("rashi", "rashi@test.com", "password123")

        result = login_user("rashi", "password123")

        self.assertIsNotNone(result["token"])
        self.assertEqual(result["username"], "rashi")

    def test_wrong_password(self):
        register_user("rashi", "rashi@test.com", "password123")

        with self.assertRaises(ValueError):
            login_user("rashi", "wrongpassword")

    def test_session_and_logout(self):
        register_user("rashi", "rashi@test.com", "password123")
        result = login_user("rashi", "password123")

        token = result["token"]

        self.assertEqual(verify_session(token), result["user_id"])

        self.assertTrue(logout_user(token))
        self.assertIsNone(verify_session(token))


if __name__ == "__main__":
    unittest.main()