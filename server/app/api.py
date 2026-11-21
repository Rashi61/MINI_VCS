from functools import wraps

from flask import Flask, jsonify, request

from .auth import (
    login_user,
    logout_user,
    register_user,
    verify_session,
)
from .config import load_settings
from .database import (
    check_tables,
    initialize_database,
)
from .repository_service import (
    RepositoryServiceError,
    create_repository,
    get_repository,
    list_repositories,
    run_repository_command,
    user_can_view_repository,
    run_repository_write_command,
    user_can_view_repository,
)


def success(message, data=None, status=200):
    return jsonify(
        {
            "ok": True,
            "message": message,
            "data": data or {},
        }
    ), status


def failure(message, error_code, status=400):
    return jsonify(
        {
            "ok": False,
            "message": message,
            "error_code": error_code,
        }
    ), status


def bearer_token():
    header = request.headers.get("Authorization", "")

    if not header.startswith("Bearer "):
        return None

    token = header[7:].strip()

    return token or None


def current_user_id():
    token = bearer_token()

    if token is None:
        return None

    return verify_session(token)


def require_login(handler):
    @wraps(handler)
    def wrapped(*args, **kwargs):
        user_id = current_user_id()

        if user_id is None:
            return failure(
                "Login is required.",
                "AUTHENTICATION_REQUIRED",
                401,
            )

        return handler(user_id, *args, **kwargs)

    return wrapped


def create_app() -> Flask:
    app = Flask(__name__)

    settings = load_settings()

    app.config["MINIVCS_SETTINGS"] = settings

    initialize_database()

    @app.get("/api/health")
    def health():
        try:
            tables = check_tables()

            return success(
                "MiniVCS API is running",
                {
                    "service": "minivcs-api",
                    "status": "healthy",
                    "version": "0.1.0",
                    "database": "ready",
                    "tables": tables,
                },
            )
        except Exception:
            return failure(
                "MiniVCS API database is unavailable.",
                "DATABASE_UNAVAILABLE",
                503,
            )

    @app.post("/api/auth/register")
    def register():
        payload = request.get_json(silent=True) or {}

        try:
            user_id = register_user(
                payload.get("username", ""),
                payload.get("email", ""),
                payload.get("password", ""),
            )

            return success(
                "Registration successful.",
                {
                    "user_id": user_id,
                },
                201,
            )

        except ValueError as error:
            return failure(
                str(error),
                "REGISTRATION_ERROR",
                400,
            )

    @app.post("/api/auth/login")
    def login():
        payload = request.get_json(silent=True) or {}

        try:
            result = login_user(
                payload.get("username", ""),
                payload.get("password", ""),
            )

            return success(
                "Login successful.",
                result,
            )

        except ValueError as error:
            return failure(
                str(error),
                "LOGIN_ERROR",
                401,
            )

    @app.post("/api/auth/logout")
    @require_login
    def logout(user_id):
        token = bearer_token()

        if not logout_user(token):
            return failure(
                "Logout failed.",
                "LOGOUT_ERROR",
                400,
            )

        return success("Logout successful.")

    @app.post("/api/repositories")
    @require_login
    def create_repository_route(user_id):
        payload = request.get_json(silent=True) or {}

        try:
            repository = create_repository(
                settings,
                owner_id=user_id,
                name=payload.get("name", ""),
                visibility=payload.get(
                    "visibility",
                    "private",
                ),
            )

            # Do not expose server filesystem paths.
            repository.pop("path", None)

            return success(
                "Repository created successfully.",
                repository,
                201,
            )

        except RepositoryServiceError as error:
            return failure(
                str(error),
                "REPOSITORY_ERROR",
                400,
            )

    @app.get("/api/repositories")
    def repositories():
        user_id = current_user_id()

        try:
            public_repositories = list_repositories(
                settings,
                visibility="public",
            )

            if user_id is None:
                visible = public_repositories
            else:
                owned_repositories = list_repositories(
                    settings,
                    owner_id=user_id,
                )

                combined = {
                    repository["id"]: repository
                    for repository in public_repositories
                }

                for repository in owned_repositories:
                    combined[repository["id"]] = repository

                visible = list(combined.values())

            return success(
                "Repositories loaded.",
                {
                    "repositories": visible,
                },
            )

        except RepositoryServiceError as error:
            return failure(
                str(error),
                "REPOSITORY_ERROR",
                400,
            )
    @app.get(
        "/api/repositories/<int:repository_id>"
    )
    def repository_details(repository_id):
        user_id = current_user_id()

        try:
            repository = get_repository(
                settings,
                repository_id,
            )

            if repository is None:
                return failure(
                    "Repository was not found.",
                    "REPOSITORY_NOT_FOUND",
                    404,
                )

            if not user_can_view_repository(
                settings,
                repository,
                user_id,
            ):
                return failure(
                    "You do not have permission to view this repository.",
                    "PERMISSION_DENIED",
                    403,
                )

            return success(
                "Repository details loaded.",
                repository,
            )

        except RepositoryServiceError as error:
            return failure(
                str(error),
                "REPOSITORY_ERROR",
                400,
            )

    @app.get(
        "/api/repositories/<int:repository_id>/status"
    )
    def repository_status(repository_id):
        user_id = current_user_id()

        try:
            result = run_repository_command(
                settings,
                repository_id,
                ["status"],
                user_id,
            )

            return success(
                "Repository status loaded.",
                {
                    "repository": result["repository"],
                    "output": result["output"],
                },
            )

        except RepositoryServiceError as error:
            status = (
                403
                if "permission" in str(error).lower()
                else 404
            )

            return failure(
                str(error),
                "REPOSITORY_STATUS_ERROR",
                status,
            )

    @app.get(
        "/api/repositories/<int:repository_id>/history"
    )
    def repository_history(repository_id):
        user_id = current_user_id()

        try:
            result = run_repository_command(
                settings,
                repository_id,
                ["history"],
                user_id,
            )

            return success(
                "Repository history loaded.",
                {
                    "repository": result["repository"],
                    "output": result["output"],
                },
            )

        except RepositoryServiceError as error:
            status = (
                403
                if "permission" in str(error).lower()
                else 404
            )

            return failure(
                str(error),
                "REPOSITORY_HISTORY_ERROR",
                status,
            )
    @app.get(
        "/api/repositories/<int:repository_id>/branches"
    )
    def repository_branches(repository_id):
        user_id = current_user_id()

        try:
            result = run_repository_command(
                settings,
                repository_id,
                ["branches"],
                user_id,
            )

            return success(
                "Repository branches loaded.",
                {
                    "repository": result["repository"],
                    "output": result["output"],
                },
            )

        except RepositoryServiceError as error:
            message = str(error)

            if "permission" in message.lower():
                status = 403
            elif "not found" in message.lower():
                status = 404
            else:
                status = 400

            return failure(
                message,
                "BRANCH_LIST_ERROR",
                status,
            )

    @app.post(
        "/api/repositories/<int:repository_id>/branches"
    )
    @require_login
    def create_repository_branch(user_id, repository_id):
        payload = request.get_json(silent=True) or {}
        branch_name = payload.get("name", "")

        if not isinstance(branch_name, str):
            return failure(
                "Branch name must be a string.",
                "VALIDATION_ERROR",
                400,
            )

        branch_name = branch_name.strip()

        if not branch_name:
            return failure(
                "Branch name is required.",
                "VALIDATION_ERROR",
                400,
            )

        if len(branch_name) > 64:
            return failure(
                "Branch name is too long.",
                "VALIDATION_ERROR",
                400,
            )

        if any(
            character.isspace()
            for character in branch_name
        ):
            return failure(
                "Branch name cannot contain spaces.",
                "VALIDATION_ERROR",
                400,
            )

        try:
            result = run_repository_write_command(
                settings,
                repository_id,
                [
                    "branch",
                    "create",
                    branch_name,
                ],
                user_id,
                "create_branch",
            )

            return success(
                "Branch created successfully.",
                {
                    "repository": result["repository"],
                    "output": result["output"],
                },
                201,
            )

        except RepositoryServiceError as error:
            message = str(error)

            if "permission" in message.lower():
                status = 403
            elif "not found" in message.lower():
                status = 404
            else:
                status = 400

            return failure(
                message,
                "BRANCH_CREATE_ERROR",
                status,
            )
    @app.post(
        "/api/repositories/<int:repository_id>/commit"
    )
    @require_login
    def commit_repository(user_id, repository_id):
        payload = request.get_json(silent=True) or {}
        message = payload.get("message", "")

        if not isinstance(message, str):
            return failure(
                "Commit message must be a string.",
                "VALIDATION_ERROR",
                400,
            )

        message = message.strip()

        if not message:
            return failure(
                "Commit message is required.",
                "VALIDATION_ERROR",
                400,
            )

        if len(message) > 500:
            return failure(
                "Commit message is too long.",
                "VALIDATION_ERROR",
                400,
            )

        try:
            result = run_repository_write_command(
                settings,
                repository_id,
                [
                    "commit",
                    message,
                ],
                user_id,
                "push",
            )

            return success(
                "Commit created successfully.",
                {
                    "repository": result["repository"],
                    "output": result["output"],
                },
                201,
            )

        except RepositoryServiceError as error:
            message_text = str(error)

            if "permission" in message_text.lower():
                status = 403
            elif "not found" in message_text.lower():
                status = 404
            else:
                status = 400

            return failure(
                message_text,
                "COMMIT_ERROR",
                status,
            )
        
    return app