import re
import shutil
import subprocess
import threading
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


# ---------------------------------------------------------------------------
# Per-repository locking.
# Every MiniVCS command for one repository runs one at a time, so two
# requests can never modify the same working tree concurrently.
# ---------------------------------------------------------------------------
_REPOSITORY_LOCKS = {}
_REPOSITORY_LOCKS_GUARD = threading.Lock()


def _lock_for(key):
    with _REPOSITORY_LOCKS_GUARD:
        lock = _REPOSITORY_LOCKS.get(key)

        if lock is None:
            lock = threading.RLock()
            _REPOSITORY_LOCKS[key] = lock

        return lock


def _run_locked(command, **kwargs):
    key = str(kwargs.get("cwd") or "")

    with _lock_for(key):
        return subprocess.run(command, **kwargs)


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

    completed = _run_locked(
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
        "retention",
    }

    if not arguments or arguments[0] not in allowed_commands:
        raise RepositoryServiceError(
            "Unsupported repository command."
        )

    completed = _run_locked(
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
        "merge",
        "retention",
    }

    if not arguments or arguments[0] not in allowed_commands:
        raise RepositoryServiceError(
            "Unsupported repository command."
        )

    completed = _run_locked(
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

    completed = _run_locked(
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

    completed = _run_locked(
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
MEMBER_ROLES = {
    "maintainer",
    "contributor",
    "viewer",
}


def _member_management_context(
    settings,
    repository_id,
    requester_id,
):
    repository = get_repository(
        settings,
        repository_id,
    )

    if repository is None:
        raise RepositoryServiceError(
            "Repository was not found."
        )

    if requester_id is None:
        raise RepositoryServiceError(
            "Login is required."
        )

    requester_role = get_repository_role(
        requester_id,
        repository_id,
    )

    if requester_role not in {
        "owner",
        "maintainer",
    }:
        raise RepositoryServiceError(
            "You do not have permission to manage members."
        )

    return repository, requester_role


def _validate_member_role(role):
    if not isinstance(role, str):
        raise RepositoryServiceError(
            "Member role must be text."
        )

    role = role.strip().lower()

    if role not in MEMBER_ROLES:
        raise RepositoryServiceError(
            "Member role must be maintainer, contributor, or viewer."
        )

    return role


def _member_to_dict(row):
    return {
        "user_id": row["user_id"],
        "username": row["username"],
        "email": row["email"],
        "role": row["role"],
        "joined_at": row["joined_at"],
    }


def list_repository_members(
    settings,
    repository_id,
    requester_id,
):
    repository, requester_role = (
        _member_management_context(
            settings,
            repository_id,
            requester_id,
        )
    )

    connection = get_connection(
        settings.database_path
    )

    try:
        rows = connection.execute(
            """
            SELECT
                users.id AS user_id,
                users.username,
                users.email,
                CASE
                    WHEN users.id = repositories.owner_id
                    THEN 'owner'
                    ELSE repository_members.role
                END AS role,
                repository_members.joined_at
            FROM repositories
            JOIN users
                ON users.id = repositories.owner_id
            LEFT JOIN repository_members
                ON repository_members.repository_id =
                   repositories.id
               AND repository_members.user_id =
                   users.id
            WHERE repositories.id = ?

            UNION ALL

            SELECT
                users.id AS user_id,
                users.username,
                users.email,
                repository_members.role,
                repository_members.joined_at
            FROM repository_members
            JOIN users
                ON users.id = repository_members.user_id
            JOIN repositories
                ON repositories.id =
                   repository_members.repository_id
            WHERE repository_members.repository_id = ?
              AND repository_members.user_id != repositories.owner_id

            ORDER BY role, username
            """,
            (
                repository_id,
                repository_id,
            ),
        ).fetchall()

        return {
            "repository": repository,
            "members": [
                _member_to_dict(row)
                for row in rows
            ],
        }

    finally:
        connection.close()


def add_repository_member(
    settings,
    repository_id,
    requester_id,
    username,
    role,
):
    repository, requester_role = (
        _member_management_context(
            settings,
            repository_id,
            requester_id,
        )
    )

    if not isinstance(username, str):
        raise RepositoryServiceError(
            "Username must be text."
        )

    username = username.strip()

    if not username:
        raise RepositoryServiceError(
            "Username is required."
        )

    role = _validate_member_role(role)

    if (
        requester_role == "maintainer"
        and role not in {
            "contributor",
            "viewer",
        }
    ):
        raise RepositoryServiceError(
            "Maintainers can add only contributors or viewers."
        )

    connection = get_connection(
        settings.database_path
    )

    try:
        user = connection.execute(
            """
            SELECT id, username, email
            FROM users
            WHERE username = ?
            """,
            (username,),
        ).fetchone()

        if user is None:
            raise RepositoryServiceError(
                "User was not found."
            )

        if user["id"] == repository["owner_id"]:
            raise RepositoryServiceError(
                "The repository owner is already a member."
            )

        existing = connection.execute(
            """
            SELECT 1
            FROM repository_members
            WHERE repository_id = ?
              AND user_id = ?
            """,
            (
                repository_id,
                user["id"],
            ),
        ).fetchone()

        if existing is not None:
            raise RepositoryServiceError(
                "This user is already a repository member."
            )

        connection.execute(
            """
            INSERT INTO repository_members (
                repository_id,
                user_id,
                role
            )
            VALUES (?, ?, ?)
            """,
            (
                repository_id,
                user["id"],
                role,
            ),
        )

        connection.commit()

        return {
            "user_id": user["id"],
            "username": user["username"],
            "email": user["email"],
            "role": role,
        }

    finally:
        connection.close()


def update_repository_member(
    settings,
    repository_id,
    requester_id,
    member_user_id,
    role,
):
    repository, requester_role = (
        _member_management_context(
            settings,
            repository_id,
            requester_id,
        )
    )

    if not isinstance(member_user_id, int):
        raise RepositoryServiceError(
            "Member user ID must be an integer."
        )

    role = _validate_member_role(role)

    if member_user_id == repository["owner_id"]:
        raise RepositoryServiceError(
            "The repository owner role cannot be changed."
        )

    if (
        requester_role == "maintainer"
        and role not in {
            "contributor",
            "viewer",
        }
    ):
        raise RepositoryServiceError(
            "Maintainers can assign only contributors or viewers."
        )

    connection = get_connection(
        settings.database_path
    )

    try:
        member = connection.execute(
            """
            SELECT
                users.id AS user_id,
                users.username,
                users.email,
                repository_members.role,
                repository_members.joined_at
            FROM repository_members
            JOIN users
                ON users.id = repository_members.user_id
            WHERE repository_members.repository_id = ?
              AND repository_members.user_id = ?
            """,
            (
                repository_id,
                member_user_id,
            ),
        ).fetchone()

        if member is None:
            raise RepositoryServiceError(
                "Repository member was not found."
            )

        connection.execute(
            """
            UPDATE repository_members
            SET role = ?
            WHERE repository_id = ?
              AND user_id = ?
            """,
            (
                role,
                repository_id,
                member_user_id,
            ),
        )

        connection.commit()

        return {
            "user_id": member["user_id"],
            "username": member["username"],
            "email": member["email"],
            "role": role,
            "joined_at": member["joined_at"],
        }

    finally:
        connection.close()


def remove_repository_member(
    settings,
    repository_id,
    requester_id,
    member_user_id,
):
    repository, requester_role = (
        _member_management_context(
            settings,
            repository_id,
            requester_id,
        )
    )

    if not isinstance(member_user_id, int):
        raise RepositoryServiceError(
            "Member user ID must be an integer."
        )

    if member_user_id == repository["owner_id"]:
        raise RepositoryServiceError(
            "The repository owner cannot be removed."
        )

    connection = get_connection(
        settings.database_path
    )

    try:
        member = connection.execute(
            """
            SELECT users.username
            FROM repository_members
            JOIN users
                ON users.id = repository_members.user_id
            WHERE repository_members.repository_id = ?
              AND repository_members.user_id = ?
            """,
            (
                repository_id,
                member_user_id,
            ),
        ).fetchone()

        if member is None:
            raise RepositoryServiceError(
                "Repository member was not found."
            )

        connection.execute(
            """
            DELETE FROM repository_members
            WHERE repository_id = ?
              AND user_id = ?
            """,
            (
                repository_id,
                member_user_id,
            ),
        )

        connection.commit()

        return {
            "user_id": member_user_id,
            "username": member["username"],
            "removed": True,
        }

    finally:
        connection.close()

# ---------------------------------------------------------------------------
# Merge, retention and structured history
# ---------------------------------------------------------------------------
BRANCH_NAME_PATTERN = re.compile(
    r"^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$"
)

MIN_RETENTION_DAYS = 1
MAX_RETENTION_DAYS = 3650

_HISTORY_KEYS = {
    "commit": "id",
    "message": "message",
    "date": "date",
    "parent": "parent",
}

_RETENTION_HEADER = re.compile(
    r"^Retention:\s*(\d+)\s+days?",
    re.IGNORECASE,
)

_RETENTION_LINE = re.compile(
    r"^\s+([0-9a-fA-F]{6,64})\s+"
    r"(\d{4}-\d{2}-\d{2} \d{2}:\d{2})\s+"
    r"(-?\d+)d\s+(.+?)\s*$"
)


def validate_branch_name(name) -> str:
    if not isinstance(name, str):
        raise RepositoryServiceError(
            "Branch name must be text."
        )

    normalized = name.strip()

    if not BRANCH_NAME_PATTERN.fullmatch(normalized) or ".." in normalized:
        raise RepositoryServiceError(
            "Branch name may only contain letters, numbers, dots, "
            "dashes and underscores (maximum 64 characters)."
        )

    return normalized


def validate_retention_days(days) -> int:
    try:
        value = int(days)
    except (TypeError, ValueError):
        raise RepositoryServiceError(
            "Retention days must be a whole number."
        )

    if value < MIN_RETENTION_DAYS or value > MAX_RETENTION_DAYS:
        raise RepositoryServiceError(
            f"Retention days must be between {MIN_RETENTION_DAYS} "
            f"and {MAX_RETENTION_DAYS}."
        )

    return value


def parse_history_output(text: str) -> list:
    commits = []
    current = {}

    for line in (text or "").splitlines():
        if not line.strip():
            if current:
                commits.append(current)
                current = {}
            continue

        key, separator, value = line.partition(":")

        if not separator:
            continue

        mapped = _HISTORY_KEYS.get(key.strip().lower())

        if mapped is None:
            continue

        value = value.strip()

        if mapped == "id" and current:
            commits.append(current)
            current = {}

        current[mapped] = value

    if current:
        commits.append(current)

    for commit in commits:
        parent = commit.get("parent", "")
        commit["parent"] = None if parent in {"", "none"} else parent

    return commits


def parse_retention_output(text: str) -> dict:
    days = None
    snapshots = []

    for line in (text or "").splitlines():
        header = _RETENTION_HEADER.match(line.strip())

        if header:
            days = int(header.group(1))
            continue

        match = _RETENTION_LINE.match(line)

        if match:
            age = int(match.group(3))
            snapshots.append(
                {
                    "id": match.group(1),
                    "date": match.group(2),
                    "age_days": age,
                    "result": match.group(4),
                }
            )

    return {
        "days": days,
        "snapshots": snapshots,
        "raw": text or "",
    }


def get_structured_history(settings, repository_id, user_id) -> dict:
    result = run_repository_command(
        settings,
        repository_id,
        ["history"],
        user_id,
    )

    return {
        "repository": result["repository"],
        "commits": parse_history_output(result["output"]),
    }


def get_retention_status(settings, repository_id, user_id) -> dict:
    result = run_repository_command(
        settings,
        repository_id,
        ["retention", "status"],
        user_id,
    )

    parsed = parse_retention_output(result["output"])
    parsed["repository"] = result["repository"]
    return parsed


def set_retention_days(settings, repository_id, days, user_id) -> dict:
    value = validate_retention_days(days)

    return run_repository_write_command(
        settings,
        repository_id,
        ["retention", "set", str(value)],
        user_id,
        "change_settings",
    )


def run_retention_cleanup(settings, repository_id, user_id) -> dict:
    return run_repository_write_command(
        settings,
        repository_id,
        ["retention", "cleanup"],
        user_id,
        "change_settings",
    )


def merge_branch(settings, repository_id, source_branch, user_id) -> dict:
    branch = validate_branch_name(source_branch)

    return run_repository_write_command(
        settings,
        repository_id,
        ["merge", branch],
        user_id,
        "push",
    )
