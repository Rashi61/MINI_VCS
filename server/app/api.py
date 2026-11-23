import os
from functools import wraps

from flask import Flask, jsonify, request, send_from_directory
from werkzeug.exceptions import HTTPException
from werkzeug.middleware.proxy_fix import ProxyFix

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
    add_repository_member,
    create_repository,
    get_repository,
    get_retention_status,
    get_structured_history,
    merge_branch,
    run_retention_cleanup,
    set_retention_days,
    list_public_repository_files,
    list_repository_members,
    list_repositories,
    read_public_repository_file,
    remove_repository_member,
    run_remote_operation,
    run_repository_command,
    run_repository_write_command,
    save_and_stage_file,
    update_repository_member,
    user_can_view_repository,
)
from flask_cors import CORS

from .security import RateLimiter, apply_security_headers

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
def repository_error_status(error):
    message = str(error).lower()

    if "permission" in message:
        return 403

    if "not found" in message:
        return 404

    if "missing" in message:
        return 404

    if "outside" in message:
        return 400

    if "parent" in message:
        return 400

    if "absolute" in message:
        return 400

    if "metadata" in message:
        return 400

    if "directory" in message:
        return 400

    if "file" in message:
        return 400

    return 400

def _service_status(error, default=400):
    message = str(error).lower()

    if "permission" in message:
        return 403

    if "not found" in message or "missing" in message:
        return 404

    if "conflict" in message:
        return 409

    return default


def create_app() -> Flask:
    settings = load_settings()

    app = Flask(__name__, static_folder=None)
    app.config["MAX_CONTENT_LENGTH"] = 10 * 1024 * 1024

    if os.getenv("MINIVCS_TRUST_PROXY", "").strip().lower() in {
        "1", "true", "yes", "on",
    }:
        # Render, Railway and similar hosts terminate HTTPS in front of us.
        app.wsgi_app = ProxyFix(
            app.wsgi_app,
            x_for=1,
            x_proto=1,
            x_host=1,
        )

    CORS(
        app,
        origins=list(settings.allowed_origins),
        supports_credentials=False,
        allow_headers=["Content-Type", "Authorization"],
        methods=["GET",
                "POST",
                "PATCH",
                "DELETE",
                "OPTIONS",],
    )

    app.config["MINIVCS_SETTINGS"] = settings

    login_limiter = RateLimiter(
        settings.login_max_attempts,
        settings.login_window_seconds,
    )
    register_limiter = RateLimiter(
        settings.register_max_attempts,
        settings.register_window_seconds,
    )

    @app.after_request
    def add_security_headers(response):
        return apply_security_headers(
            response,
            secure_transport=request.is_secure,
        )

    @app.errorhandler(HTTPException)
    def handle_http_error(error):
        if request.path.startswith("/api/"):
            if error.code == 413:
                return failure(
                    "The request is too large (10 MB limit).",
                    "REQUEST_TOO_LARGE",
                    413,
                )

            return failure(
                error.description or "Request failed.",
                "HTTP_ERROR",
                error.code or 500,
            )

        return error

    @app.errorhandler(Exception)
    def handle_unexpected_error(error):
        app.logger.exception("Unhandled server error")

        if request.path.startswith("/api/"):
            return failure(
                "Unexpected server error.",
                "INTERNAL_ERROR",
                500,
            )

        return "Internal server error", 500

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

        register_key = request.remote_addr or "unknown"

        if register_limiter.is_blocked(register_key):
            return failure(
                "Too many registration attempts. Try again later.",
                "RATE_LIMITED",
                429,
            )

        register_limiter.register_failure(register_key)

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

        login_name = str(payload.get("username", "")).strip().lower()
        login_key = f"{request.remote_addr or 'unknown'}:{login_name}"

        if login_limiter.is_blocked(login_key):
            response, status = failure(
                "Too many failed login attempts. Try again later.",
                "RATE_LIMITED",
                429,
            )
            response.headers["Retry-After"] = str(
                login_limiter.retry_after(login_key)
            )
            return response, status

        try:
            result = login_user(
                payload.get("username", ""),
                payload.get("password", ""),
            )

            login_limiter.reset(login_key)

            return success(
                "Login successful.",
                result,
            )

        except ValueError as error:
            login_limiter.register_failure(login_key)

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
        payload = (
            request.get_json(
                silent=True
            )
            or {}
        )

        name = payload.get(
            "name",
            "",
        )

        description = payload.get(
            "description",
            "",
        )

        visibility = payload.get(
            "visibility",
            "private",
        )

        if not isinstance(name, str):
            return failure(
                "Repository name must be text.",
                "INVALID_NAME",
                400,
            )

        if not isinstance(description, str):
            return failure(
                "Description must be text.",
                "INVALID_DESCRIPTION",
                400,
            )

        if not isinstance(visibility, str):
            return failure(
                "Visibility must be text.",
                "INVALID_VISIBILITY",
                400,
            )

        name = name.strip()
        description = description.strip()
        visibility = visibility.strip().lower()

        if not name:
            return failure(
                "Repository name is required.",
                "INVALID_NAME",
                400,
            )

        if len(description) > 500:
            return failure(
                "Repository description must be 500 characters or fewer.",
                "INVALID_DESCRIPTION",
                400,
            )

        if visibility not in {
            "public",
            "private",
        }:
            return failure(
                "Visibility must be public or private.",
                "INVALID_VISIBILITY",
                400,
            )

        try:
            repository = create_repository(
                settings=settings,
                owner_id=user_id,
                name=name,
                description=description,
                visibility=visibility,
            )

            if isinstance(repository, dict):
                repository.pop(
                    "path",
                    None,
                )

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

        can_view = (
            repository["visibility"] == "public"
        )

        if not can_view and user_id is not None:
            can_view = user_can_view_repository(
                settings,
                repository,
                user_id,
            )

        if not can_view:
            return failure(
                "You do not have permission to view this repository.",
                "PRIVATE_REPOSITORY",
                403,
            )

        return success(
            "Repository details loaded.",
            repository,
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
        "/api/repositories/<int:repository_id>/commits"
    )
    def repository_commits(repository_id):
        try:
            data = get_structured_history(
                settings,
                repository_id,
                current_user_id(),
            )

            return success("Commit history loaded.", data)

        except RepositoryServiceError as error:
            return failure(
                str(error),
                "REPOSITORY_COMMITS_ERROR",
                _service_status(error, 404),
            )

    @app.get(
        "/api/repositories/<int:repository_id>/retention"
    )
    def repository_retention(repository_id):
        try:
            data = get_retention_status(
                settings,
                repository_id,
                current_user_id(),
            )

            return success("Retention status loaded.", data)

        except RepositoryServiceError as error:
            return failure(
                str(error),
                "RETENTION_STATUS_ERROR",
                _service_status(error, 404),
            )

    @app.post(
        "/api/repositories/<int:repository_id>/retention"
    )
    @require_login
    def update_repository_retention(user_id, repository_id):
        payload = request.get_json(silent=True) or {}

        try:
            set_retention_days(
                settings,
                repository_id,
                payload.get("days"),
                user_id,
            )

            data = get_retention_status(
                settings,
                repository_id,
                user_id,
            )

            return success("Retention period saved.", data)

        except RepositoryServiceError as error:
            return failure(
                str(error),
                "RETENTION_UPDATE_ERROR",
                _service_status(error),
            )

    @app.post(
        "/api/repositories/<int:repository_id>/retention/cleanup"
    )
    @require_login
    def repository_retention_cleanup(user_id, repository_id):
        try:
            result = run_retention_cleanup(
                settings,
                repository_id,
                user_id,
            )

            data = get_retention_status(
                settings,
                repository_id,
                user_id,
            )
            data["output"] = result["output"]

            return success("Retention cleanup finished.", data)

        except RepositoryServiceError as error:
            return failure(
                str(error),
                "RETENTION_CLEANUP_ERROR",
                _service_status(error),
            )

    @app.post(
        "/api/repositories/<int:repository_id>/merge"
    )
    @require_login
    def merge_repository_branch(user_id, repository_id):
        payload = request.get_json(silent=True) or {}

        try:
            result = merge_branch(
                settings,
                repository_id,
                payload.get("source", payload.get("name", "")),
                user_id,
            )

            return success(
                "Merge completed.",
                {
                    "repository": result["repository"],
                    "output": result["output"],
                },
            )

        except RepositoryServiceError as error:
            return failure(
                str(error),
                "MERGE_ERROR",
                _service_status(error),
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
    @app.post(
        "/api/repositories/<int:repository_id>/files"
    )
    @require_login
    def upload_repository_file(user_id, repository_id):
        relative_path = request.form.get(
            "path",
            "",
        )

        uploaded_file = request.files.get(
            "file"
        )

        if uploaded_file is None:
            return failure(
                "A multipart file field named 'file' is required.",
                "FILE_REQUIRED",
                400,
            )

        if not uploaded_file.filename:
            return failure(
                "The uploaded file must have a filename.",
                "FILENAME_REQUIRED",
                400,
            )

        try:
            result = save_and_stage_file(
                settings,
                repository_id,
                user_id,
                relative_path,
                uploaded_file,
            )

            return success(
                "File uploaded and staged successfully.",
                {
                    "repository": result["repository"],
                    "path": result["path"],
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
                "FILE_UPLOAD_ERROR",
                status,
            )
    @app.post(
        "/api/repositories/<int:repository_id>/branches/switch"
    )
    @require_login
    def switch_repository_branch(
        user_id,
        repository_id,
    ):
        payload = request.get_json(
            silent=True
        ) or {}

        branch_name = payload.get(
            "name",
            "",
        )

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
                    "switch",
                    branch_name,
                ],
                user_id,
                "push",
            )

            return success(
                "Branch switched successfully.",
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
                "BRANCH_SWITCH_ERROR",
                status,
            )
    @app.post(
        "/api/repositories/<int:repository_id>/remote/<operation>"
    )
    @require_login
    def repository_remote_operation(
        user_id,
        repository_id,
        operation,
    ):
        if operation not in {
            "clone",
            "push",
            "pull",
        }:
            return failure(
                "Unsupported remote operation.",
                "REMOTE_OPERATION_ERROR",
                400,
            )

        try:
            result = run_remote_operation(
                settings,
                repository_id,
                operation,
                user_id,
            )

            return success(
                f"Remote {operation} completed.",
                {
                    "repository": result["repository"],
                    "operation": result["operation"],
                    "output": result["output"],
                },
            )

        except RepositoryServiceError as error:
            message = str(error)
            lowered = message.lower()

            if "permission" in lowered:
                status = 403
            elif "not found" in lowered:
                status = 404
            else:
                status = 400

            return failure(
                message,
                "REMOTE_OPERATION_ERROR",
                status,
            )
    @app.get(
        "/api/repositories/<int:repository_id>/files"
    )
    def repository_files(repository_id):
        user_id = current_user_id()

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

        can_view = (
            repository["visibility"] == "public"
        )

        if not can_view and user_id is not None:
            can_view = user_can_view_repository(
                settings,
                repository,
                user_id,
            )

        if not can_view:
            return failure(
                "You do not have permission to view this repository.",
                "PRIVATE_REPOSITORY",
                403,
            )

        relative_path = request.args.get(
            "path",
            "",
        )

        try:
            result = list_public_repository_files(
                settings,
                repository_id,
                relative_path,
            )

            return success(
                "Repository files loaded.",
                result,
            )

        except RepositoryServiceError as error:
            return failure(
                str(error),
                "FILE_LIST_ERROR",
                repository_error_status(error),
            )

    @app.get(
        "/api/repositories/<int:repository_id>/files/<path:file_path>"
    )
    def repository_file_content(
        repository_id,
        file_path,
    ):
        user_id = current_user_id()

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

        can_view = (
            repository["visibility"] == "public"
        )

        if not can_view and user_id is not None:
            can_view = user_can_view_repository(
                settings,
                repository,
                user_id,
            )

        if not can_view:
            return failure(
                "You do not have permission to view this repository.",
                "PRIVATE_REPOSITORY",
                403,
            )

        try:
            result = read_public_repository_file(
                settings,
                repository_id,
                file_path,
            )

            return success(
                "Repository file loaded.",
                result,
            )

        except RepositoryServiceError as error:
            return failure(
                str(error),
                "FILE_READ_ERROR",
                repository_error_status(error),
            )
    @app.get(
        "/api/repositories/<int:repository_id>/members"
    )
    @require_login
    def repository_members(
        user_id,
        repository_id,
    ):
        try:
            result = list_repository_members(
                settings,
                repository_id,
                user_id,
            )

            return success(
                "Repository members loaded.",
                result,
            )

        except RepositoryServiceError as error:
            return failure(
                str(error),
                "MEMBER_LIST_ERROR",
                repository_error_status(error),
            )

    @app.post(
        "/api/repositories/<int:repository_id>/members"
    )
    @require_login
    def add_member(
        user_id,
        repository_id,
    ):
        payload = (
            request.get_json(
                silent=True
            )
            or {}
        )

        try:
            member = add_repository_member(
                settings=settings,
                repository_id=repository_id,
                requester_id=user_id,
                username=payload.get(
                    "username",
                    "",
                ),
                role=payload.get(
                    "role",
                    "viewer",
                ),
            )

            return success(
                "Repository member added.",
                {
                    "member": member,
                },
                201,
            )

        except RepositoryServiceError as error:
            return failure(
                str(error),
                "MEMBER_ADD_ERROR",
                repository_error_status(error),
            )

    @app.patch(
        "/api/repositories/<int:repository_id>/members/<int:member_user_id>"
    )
    @require_login
    def update_member(
        user_id,
        repository_id,
        member_user_id,
    ):
        payload = (
            request.get_json(
                silent=True
            )
            or {}
        )

        try:
            member = update_repository_member(
                settings=settings,
                repository_id=repository_id,
                requester_id=user_id,
                member_user_id=member_user_id,
                role=payload.get(
                    "role",
                    "",
                ),
            )

            return success(
                "Repository member role updated.",
                {
                    "member": member,
                },
            )

        except RepositoryServiceError as error:
            return failure(
                str(error),
                "MEMBER_UPDATE_ERROR",
                repository_error_status(error),
            )

    @app.delete(
        "/api/repositories/<int:repository_id>/members/<int:member_user_id>"
    )
    @require_login
    def remove_member(
        user_id,
        repository_id,
        member_user_id,
    ):
        try:
            result = remove_repository_member(
                settings=settings,
                repository_id=repository_id,
                requester_id=user_id,
                member_user_id=member_user_id,
            )

            return success(
                "Repository member removed.",
                result,
            )

        except RepositoryServiceError as error:
            return failure(
                str(error),
                "MEMBER_REMOVE_ERROR",
                repository_error_status(error),
            )
   
    frontend_dir = settings.frontend_dir

    @app.get("/")
    def home_page():
        return send_from_directory(frontend_dir, "index.html")

    @app.get("/<path:filename>")
    def frontend_file(filename):
        if filename.startswith("api/"):
            return failure("Not found.", "NOT_FOUND", 404)

        return send_from_directory(frontend_dir, filename)

    return app