import re
import shutil
import subprocess
from pathlib import Path

from .config import ServerSettings
from .database import get_connection
from .permissions import (
    get_repository_role,
    has_permission,
)

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
    description="",
) -> dict:
    repository_name = validate_repository_name(name)
    repository_visibility = validate_visibility(visibility)

    description = str(description or "").strip()

    if len(description) > 500:
        raise RepositoryServiceError(
            "Repository description must be 500 characters or fewer."
        )

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
                (name, description, owner_id, visibility)
            VALUES (?, ?, ?,?)
            """,
            (
                repository_name,
                description,
                owner_id,
                repository_visibility,
            ),
        )
        connection.commit()

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
            repositories.description,
            repositories.owner_id,
            users.username AS owner_username,
            repositories.visibility,
            repositories.created_at
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
                "description": row["description"] or "",
                "owner_id": row["owner_id"],
                "owner_username": row["owner_username"],
                "visibility": row["visibility"],
                "created_at": row["created_at"],
            }
            for row in rows
        ]
    finally:
        connection.close()
def get_repository(
    settings: ServerSettings,
    repository_id: int,
) -> dict | None:
    if not isinstance(repository_id, int) or repository_id <= 0:
        raise RepositoryServiceError(
            "Repository ID must be a positive integer."
        )

    connection = get_connection(
        settings.database_path
    )

    try:
        row = connection.execute(
            """
            SELECT
                repositories.id,
                repositories.name,
                repositories.description,
                repositories.owner_id,
                repositories.visibility,
                repositories.created_at,
                users.username AS owner_username
            FROM repositories
            JOIN users
                ON users.id = repositories.owner_id
            WHERE repositories.id = ?
            """,
            (repository_id,),
        ).fetchone()

        if row is None:
            return None

        return {
            "id": row["id"],
            "name": row["name"],
            "description": row["description"] or "",
            "owner_id": row["owner_id"],
            "owner_username": row["owner_username"],
            "visibility": row["visibility"],
            "created_at": row["created_at"],
        }
    finally:
        connection.close()


def user_can_view_repository(
    settings: ServerSettings,
    repository: dict,
    user_id: int | None,
) -> bool:
    if repository["visibility"] == "public":
        return True

    if user_id is None:
        return False

    if repository["owner_id"] == user_id:
        return True

    connection = get_connection(
        settings.database_path
    )

    try:
        member = connection.execute(
            """
            SELECT 1
            FROM repository_members
            WHERE repository_id = ?
              AND user_id = ?
            """,
            (
                repository["id"],
                user_id,
            ),
        ).fetchone()

        return member is not None
    finally:
        connection.close()


def run_repository_command(
    settings: ServerSettings,
    repository_id: int,
    arguments: list[str],
    user_id: int | None,
) -> dict:
    repository = get_repository(
        settings,
        repository_id,
    )

    if repository is None:
        raise RepositoryServiceError(
            "Repository was not found."
        )

    if not user_can_view_repository(
        settings,
        repository,
        user_id,
    ):
        raise RepositoryServiceError(
            "You do not have permission to view this repository."
        )

    path = repository_path(
        settings,
        repository_id,
    )

    if not path.is_dir():
        raise RepositoryServiceError(
            "Repository storage directory is missing."
        )

    executable = settings.minivcs_executable

    if not executable.is_file():
        raise RepositoryServiceError(
            "MiniVCS executable was not found."
        )

    allowed_commands = {
        "status",
        "history",
        "branches",
    }

    if not arguments or arguments[0] not in allowed_commands:
        raise RepositoryServiceError(
            "Unsupported repository command."
        )

    completed = subprocess.run(
        [
            str(executable),
            *arguments,
        ],
        cwd=str(path),
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
    )

    output = completed.stdout.strip()
    error_output = completed.stderr.strip()

    if completed.returncode != 0:
        raise RepositoryServiceError(
            error_output
            or output
            or "MiniVCS command failed."
        )

    return {
        "repository": repository,
        "command": arguments[0],
        "output": output,
    }
def user_can_perform(
    settings: ServerSettings,
    repository: dict,
    user_id: int | None,
    action: str,
) -> bool:
    if user_id is None:
        return False

    role = get_repository_role(
        user_id,
        repository["id"],
    )

    if role is None:
        return False

    return has_permission(
        role,
        action,
    )


def run_repository_write_command(
    settings: ServerSettings,
    repository_id: int,
    arguments: list[str],
    user_id: int | None,
    permission: str,
) -> dict:
    repository = get_repository(
        settings,
        repository_id,
    )

    if repository is None:
        raise RepositoryServiceError(
            "Repository was not found."
        )

    if not user_can_perform(
        settings,
        repository,
        user_id,
        permission,
    ):
        raise RepositoryServiceError(
            "You do not have permission to perform this operation."
        )

    path = repository_path(
        settings,
        repository_id,
    )

    if not path.is_dir():
        raise RepositoryServiceError(
            "Repository storage directory is missing."
        )

    executable = settings.minivcs_executable

    if not executable.is_file():
        raise RepositoryServiceError(
            "MiniVCS executable was not found."
        )

    allowed_commands = {
        "commit",
        "branch",
        "switch",
    }

    if not arguments or arguments[0] not in allowed_commands:
        raise RepositoryServiceError(
            "Unsupported repository command."
        )

    completed = subprocess.run(
        [
            str(executable),
            *arguments,
        ],
        cwd=str(path),
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
    )

    output = completed.stdout.strip()
    error_output = completed.stderr.strip()

    if completed.returncode != 0:
        raise RepositoryServiceError(
            error_output
            or output
            or "MiniVCS command failed."
        )

    return {
        "repository": repository,
        "command": arguments[0],
        "output": output,
    }
def validate_repository_file_path(
    relative_path: str,
) -> Path:
    if not isinstance(relative_path, str):
        raise RepositoryServiceError(
            "File path must be a string."
        )

    cleaned = relative_path.strip()

    if not cleaned:
        raise RepositoryServiceError(
            "File path is required."
        )

    candidate = Path(cleaned)

    if candidate.is_absolute():
        raise RepositoryServiceError(
            "Absolute file paths are not allowed."
        )

    if any(
        part in {"..", ""}
        for part in candidate.parts
    ):
        raise RepositoryServiceError(
            "Parent-directory paths are not allowed."
        )

    if ".minivcs" in candidate.parts:
        raise RepositoryServiceError(
            "The .minivcs directory cannot be modified through the API."
        )

    if len(cleaned) > 255:
        raise RepositoryServiceError(
            "File path is too long."
        )

    return candidate


def save_and_stage_file(
    settings: ServerSettings,
    repository_id: int,
    user_id: int | None,
    relative_path: str,
    uploaded_file,
) -> dict:
    repository = get_repository(
        settings,
        repository_id,
    )

    if repository is None:
        raise RepositoryServiceError(
            "Repository was not found."
        )

    if not user_can_perform(
        settings,
        repository,
        user_id,
        "push",
    ):
        raise RepositoryServiceError(
            "You do not have permission to upload files."
        )

    if uploaded_file is None:
        raise RepositoryServiceError(
            "A file is required."
        )

    safe_relative_path = validate_repository_file_path(
        relative_path
    )

    repository_root = repository_path(
        settings,
        repository_id,
    )

    target = (
        repository_root / safe_relative_path
    ).resolve()

    root = repository_root.resolve()

    try:
        target.relative_to(root)
    except ValueError as error:
        raise RepositoryServiceError(
            "File path escapes the repository."
        ) from error

    target.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    uploaded_file.save(target)

    executable = settings.minivcs_executable

    if not executable.is_file():
        raise RepositoryServiceError(
            "MiniVCS executable was not found."
        )

    completed = subprocess.run(
        [
            str(executable),
            "add",
            safe_relative_path.as_posix(),
        ],
        cwd=str(repository_root),
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
    )

    output = completed.stdout.strip()
    error_output = completed.stderr.strip()

    if completed.returncode != 0:
        raise RepositoryServiceError(
            error_output
            or output
            or "File was saved but could not be staged."
        )

    return {
        "repository": repository,
        "path": safe_relative_path.as_posix(),
        "output": output,
    }
def remote_path(
    settings: ServerSettings,
    repository_id: int,
) -> Path:
    if not isinstance(repository_id, int) or repository_id <= 0:
        raise RepositoryServiceError(
            "Repository ID must be a positive integer."
        )

    root = settings.remote_root.resolve()
    path = (
        root / str(repository_id)
    ).resolve()

    if path.parent != root:
        raise RepositoryServiceError(
            "Invalid remote storage location."
        )

    return path


def run_remote_operation(
    settings: ServerSettings,
    repository_id: int,
    operation: str,
    user_id: int | None,
) -> dict:
    repository = get_repository(
        settings,
        repository_id,
    )

    if repository is None:
        raise RepositoryServiceError(
            "Repository was not found."
        )

    allowed_operations = {
        "clone",
        "push",
        "pull",
    }

    if operation not in allowed_operations:
        raise RepositoryServiceError(
            "Unsupported remote operation."
        )

    if operation == "push":
        required_permission = "push"
    else:
        required_permission = "clone"

    if not user_can_perform(
        settings,
        repository,
        user_id,
        required_permission,
    ):
        raise RepositoryServiceError(
            "You do not have permission for this remote operation."
        )

    repository_root = repository_path(
        settings,
        repository_id,
    )

    if not repository_root.is_dir():
        raise RepositoryServiceError(
            "Repository storage directory is missing."
        )

    configured_remote = remote_path(
        settings,
        repository_id,
    )

    configured_remote.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    executable = settings.minivcs_executable

    if not executable.is_file():
        raise RepositoryServiceError(
            "MiniVCS executable was not found."
        )

    completed = subprocess.run(
        [
            str(executable),
            "remote",
            operation,
            str(configured_remote),
        ],
        cwd=str(repository_root),
        capture_output=True,
        text=True,
        timeout=60,
        check=False,
    )

    output = completed.stdout.strip()
    error_output = completed.stderr.strip()

    if completed.returncode != 0:
        raise RepositoryServiceError(
            error_output
            or output
            or "Remote operation failed."
        )

    return {
        "repository": repository,
        "operation": operation,
        "output": output,
    }
MAX_PUBLIC_FILE_SIZE = 512 * 1024


def _safe_repository_file_path(
    settings,
    repository_id,
    relative_path,
):
    if not isinstance(relative_path, str):
        raise RepositoryServiceError(
            "File path must be text."
        )

    relative_path = relative_path.strip()

    if not relative_path:
        raise RepositoryServiceError(
            "File path is required."
        )

    candidate_path = Path(relative_path)

    if candidate_path.is_absolute():
        raise RepositoryServiceError(
            "Absolute file paths are not allowed."
        )

    parts = candidate_path.parts

    if ".." in parts:
        raise RepositoryServiceError(
            "Parent-directory paths are not allowed."
        )

    if ".minivcs" in parts:
        raise RepositoryServiceError(
            "MiniVCS metadata cannot be accessed."
        )

    repository_root = (
        settings.repository_root / str(repository_id)
    ).resolve()

    target_path = (
        repository_root / candidate_path
    ).resolve()

    try:
        target_path.relative_to(repository_root)
    except ValueError as error:
        raise RepositoryServiceError(
            "The requested path is outside the repository."
        ) from error

    return target_path


def list_public_repository_files(
    settings,
    repository_id,
    relative_path="",
):
    repository = get_repository(
        settings,
        repository_id,
    )

    if repository is None:
        raise RepositoryServiceError(
            "Repository was not found."
        )

    repository_root = (
        settings.repository_root / str(repository_id)
    ).resolve()

    if not repository_root.is_dir():
        raise RepositoryServiceError(
            "Repository storage directory is missing."
        )

    if relative_path:
        directory = _safe_repository_file_path(
            settings,
            repository_id,
            relative_path,
        )
    else:
        directory = repository_root

    if not directory.exists():
        raise RepositoryServiceError(
            "The requested directory was not found."
        )

    if not directory.is_dir():
        raise RepositoryServiceError(
            "The requested path is not a directory."
        )

    entries = []

    for entry in sorted(
        directory.iterdir(),
        key=lambda item: (
            not item.is_dir(),
            item.name.lower(),
        ),
    ):
        if entry.name == ".minivcs":
            continue

        relative_entry = entry.relative_to(
            repository_root
        )

        entries.append(
            {
                "name": entry.name,
                "path": relative_entry.as_posix(),
                "type": (
                    "directory"
                    if entry.is_dir()
                    else "file"
                ),
                "size": (
                    entry.stat().st_size
                    if entry.is_file()
                    else None
                ),
            }
        )

    return {
        "repository": repository,
        "path": relative_path,
        "entries": entries,
    }


def read_public_repository_file(
    settings,
    repository_id,
    relative_path,
):
    repository = get_repository(
        settings,
        repository_id,
    )

    if repository is None:
        raise RepositoryServiceError(
            "Repository was not found."
        )

    file_path = _safe_repository_file_path(
        settings,
        repository_id,
        relative_path,
    )

    if not file_path.exists():
        raise RepositoryServiceError(
            "The requested file was not found."
        )

    if not file_path.is_file():
        raise RepositoryServiceError(
            "The requested path is not a file."
        )

    file_size = file_path.stat().st_size

    if file_size > MAX_PUBLIC_FILE_SIZE:
        raise RepositoryServiceError(
            "The requested file is too large to display."
        )

    try:
        content = file_path.read_text(
            encoding="utf-8"
        )
    except UnicodeDecodeError as error:
        raise RepositoryServiceError(
            "The requested file is not a UTF-8 text file."
        ) from error

    return {
        "repository": repository,
        "path": relative_path,
        "size": file_size,
        "content": content,
    }