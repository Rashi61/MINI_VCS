import shutil
import tempfile
import unittest
from pathlib import Path

from server.app.database import get_connection
from server.app.repository_service import (
    RepositoryServiceError,
    create_repository,
    list_repositories,
)
from server.app.config import ServerSettings, load_settings


class RepositoryServiceTests(unittest.TestCase):
    def setUp(self):
        self.temp_dir = Path(
            tempfile.mkdtemp(
                prefix="minivcs-server-test-"
            )
        )

        self.database_path = (
            self.temp_dir / "test.db"
        )

        self.repository_root = (
            self.temp_dir / "repositories"
        )

        # Uses MINIVCS_EXECUTABLE if set, otherwise the default build output
        # (works on Windows, Linux and macOS).
        self.executable = load_settings().minivcs_executable

        self.settings = ServerSettings(
            host="127.0.0.1",
            port=8000,
            debug=False,
            frontend_origin="http://127.0.0.1:5500",
            project_root=self.temp_dir,
            repository_root=self.repository_root,
            remote_root=self.temp_dir / "remotes",
            database_path=self.database_path,
            minivcs_executable=self.executable,
        )

        self._initialize_test_database()

    def tearDown(self):
        shutil.rmtree(
            self.temp_dir,
            ignore_errors=True,
        )

    def _initialize_test_database(self):
        connection = get_connection(
            self.database_path
        )

        try:
            connection.executescript(
                """
                PRAGMA foreign_keys = ON;

                CREATE TABLE users (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    username TEXT NOT NULL UNIQUE,
                    email TEXT NOT NULL UNIQUE,
                    password_hash TEXT NOT NULL,
                    password_salt TEXT NOT NULL,
                    created_at TEXT NOT NULL
                        DEFAULT CURRENT_TIMESTAMP
                );

                CREATE TABLE repositories (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    name TEXT NOT NULL,
                    description TEXT NOT NULL DEFAULT '',
                    owner_id INTEGER NOT NULL,
                    visibility TEXT NOT NULL
                        CHECK (visibility IN ('private', 'public')),
                    created_at TEXT NOT NULL
                        DEFAULT CURRENT_TIMESTAMP,
                    FOREIGN KEY (owner_id)
                        REFERENCES users(id)
                );

                CREATE TABLE repository_members (
                    repository_id INTEGER NOT NULL,
                    user_id INTEGER NOT NULL,
                    role TEXT NOT NULL,
                    joined_at TEXT NOT NULL
                        DEFAULT CURRENT_TIMESTAMP,
                    PRIMARY KEY (repository_id, user_id),
                    FOREIGN KEY (repository_id)
                        REFERENCES repositories(id),
                    FOREIGN KEY (user_id)
                        REFERENCES users(id)
                );
                """
            )

            connection.execute(
                """
                INSERT INTO users
                    (username, email, password_hash, password_salt)
                VALUES (?, ?, ?, ?)
                """,
                (
                    "owner",
                    "owner@example.com",
                    "test-hash",
                    "test-salt",
                ),
            )

            connection.commit()
        finally:
            connection.close()

    def test_create_repository(self):
        repository = create_repository(
            self.settings,
            owner_id=1,
            name="demo-project",
            visibility="public",
        )

        self.assertEqual(
            repository["name"],
            "demo-project",
        )

        self.assertEqual(
            repository["visibility"],
            "public",
        )

        self.assertTrue(
            Path(repository["path"]).is_dir()
        )

        self.assertTrue(
            Path(repository["path"], ".minivcs").is_dir()
        )

        repositories = list_repositories(
            self.settings,
            visibility="public",
        )

        self.assertEqual(
            len(repositories),
            1,
        )

    def test_rejects_unsafe_repository_name(self):
        with self.assertRaises(
            RepositoryServiceError
        ):
            create_repository(
                self.settings,
                owner_id=1,
                name="../outside",
                visibility="public",
            )

    def test_rejects_invalid_visibility(self):
        with self.assertRaises(
            RepositoryServiceError
        ):
            create_repository(
                self.settings,
                owner_id=1,
                name="demo",
                visibility="internal",
            )


if __name__ == "__main__":
    unittest.main()