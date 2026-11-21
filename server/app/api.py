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
    list_repositories,
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

    return app