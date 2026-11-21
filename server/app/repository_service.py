import re
import shutil
import subprocess
from pathlib import Path

from .config import ServerSettings
from .database import get_connection


REPOSITORY_NAME_PATTERN = re.compile(
    r"^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$"
)

ALLOWED_VISIBILITIES = {"public", "private"}


class RepositoryServiceError(Exception):
    """Expected repository-service error."""


def validate_repository_name(name: str) -> str:
    if not isinstance(name, str):
        raise RepositoryServiceError(
            "Repository name must be a string."
        )

    normalized = name.strip()

    if not normalized:
        raise RepositoryServiceError(
            "Repository name is required."
        )

    if not REPOSITORY_NAME_PATTERN.fullmatch(normalized):
        raise RepositoryServiceError(
            "Repository name may contain only letters, numbers, "
            "dots, underscores, and hyphens."
        )

    return normalized


def validate_visibility(visibility: str) -> str:
    if visibility not in ALLOWED_VISIBILITIES:
        raise RepositoryServiceError(
            "Visibility must be either 'public' or 'private'."
        )

    return visibility


def repository_path(
    settings: ServerSettings,
    repository_id: int,
) -> Path:
    root = settings.repository_root.resolve()
    path = (root / str(repository_id)).resolve()

    if path.parent != root:
        raise RepositoryServiceError(
            "Invalid repository storage location."
        )

    return path


def _initialize_minivcs_repository(
    settings: ServerSettings,
    path: Path,
) -> None:
    path.mkdir(
        parents=True,
        exist_ok=True,
    )
    
    executable = settings.minivcs_executable
    
    if not executable.is_file():
        raise RepositoryServiceError(
            "MiniVCS executable was not found. Build the C++ backend first."
        )

    completed = subprocess.run(
        [
            str(executable),
            "init",
            str(path),
        ],
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
    )

    if completed.returncode != 0:
        stderr = completed.stderr.strip()
        stdout = completed.stdout.strip()

        raise RepositoryServiceError(
            "MiniVCS repository initialization failed. "
            f"Return code: {completed.returncode}. "
            f"Executable: {executable}. "
            f"Repository path: {path}. "
            f"STDOUT: {stdout or '<empty>'}. "
            f"STDERR: {stderr or '<empty>'}."
        )


def create_repository(
    settings: ServerSettings,
    owner_id: int,
    name: str,
    visibility: str,
) -> dict:
    repository_name = validate_repository_name(name)
    repository_visibility = validate_visibility(visibility)

    if not isinstance(owner_id, int) or owner_id <= 0:
        raise RepositoryServiceError(
            "A valid owner is required."
        )

    settings.repository_root.mkdir(
        parents=True,
        exist_ok=True,
    )

    connection = get_connection(
        settings.database_path
    )

    try:
        owner = connection.execute(
            "SELECT id, username FROM users WHERE id = ?",
            (owner_id,),
        ).fetchone()

        if owner is None:
            raise RepositoryServiceError(
                "The repository owner does not exist."
            )

        duplicate = connection.execute(
            """
            SELECT id
            FROM repositories
            WHERE owner_id = ? AND name = ?
            """,
            (owner_id, repository_name),
        ).fetchone()

        if duplicate is not None:
            raise RepositoryServiceError(
                "You already have a repository with this name."
            )

        cursor = connection.execute(
            """
            INSERT INTO repositories
                (name, owner_id, visibility)
            VALUES (?, ?, ?)
            """,
            (
                repository_name,
                owner_id,
                repository_visibility,
            ),
        )

        repository_id = int(cursor.lastrowid)
        storage_path = repository_path(
            settings,
            repository_id,
        )

        try:
            _initialize_minivcs_repository(
                settings,
                storage_path,
            )

            connection.execute(
                """
                INSERT INTO repository_members
                    (repository_id, user_id, role)
                VALUES (?, ?, 'owner')
                """,
                (repository_id, owner_id),
            )

            connection.commit()

            return {
                "id": repository_id,
                "name": repository_name,
                "owner_id": owner_id,
                "visibility": repository_visibility,
                "path": str(storage_path),
            }

        except Exception:
            connection.rollback()

            if storage_path.exists():
                shutil.rmtree(
                    storage_path,
                    ignore_errors=True,
                )

            raise

    finally:
        connection.close()


def list_repositories(
    settings: ServerSettings,
    visibility: str | None = None,
    owner_id: int | None = None,
) -> list[dict]:
    query = """
        SELECT
            repositories.id,
            repositories.name,
            repositories.owner_id,
            repositories.visibility,
            repositories.created_at,
            users.username AS owner_username
        FROM repositories
        JOIN users
            ON users.id = repositories.owner_id
    """

    conditions = []
    parameters = []

    if visibility is not None:
        visibility = validate_visibility(visibility)
        conditions.append(
            "repositories.visibility = ?"
        )
        parameters.append(visibility)

    if owner_id is not None:
        if not isinstance(owner_id, int) or owner_id <= 0:
            raise RepositoryServiceError(
                "Invalid owner ID."
            )

        conditions.append(
            "repositories.owner_id = ?"
        )
        parameters.append(owner_id)

    if conditions:
        query += " WHERE " + " AND ".join(conditions)

    query += """
        ORDER BY repositories.created_at DESC,
                 repositories.id DESC
    """

    connection = get_connection(
        settings.database_path
    )

    try:
        rows = connection.execute(
            query,
            tuple(parameters),
        ).fetchall()

        return [
            {
                "id": row["id"],
                "name": row["name"],
                "owner_id": row["owner_id"],
                "owner_username": row["owner_username"],
                "visibility": row["visibility"],
                "created_at": row["created_at"],
            }
            for row in rows
        ]
    finally:
        connection.close()