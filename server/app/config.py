import os
from dataclasses import dataclass
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]


def _read_bool(name: str, default: bool = False) -> bool:
    value = os.getenv(name)

    if value is None:
        return default

    return value.strip().lower() in {
        "1",
        "true",
        "yes",
        "on",
    }


def _read_int(name: str, default: int) -> int:
    value = os.getenv(name)

    if value is None or not value.strip():
        return default

    try:
        return int(value)
    except ValueError:
        return default


def _default_executable() -> Path:
    """Find the MiniVCS binary on Windows, Linux and macOS."""
    candidates = [
        PROJECT_ROOT / "build" / "Debug" / "minivcs.exe",
        PROJECT_ROOT / "build" / "Release" / "minivcs.exe",
        PROJECT_ROOT / "build" / "minivcs.exe",
        PROJECT_ROOT / "build" / "minivcs",
        Path("/usr/local/bin/minivcs"),
    ]

    for candidate in candidates:
        if candidate.is_file():
            return candidate

    # Nothing built yet: keep a sensible platform default so the
    # error message points at the right place.
    if os.name == "nt":
        return candidates[0]

    return candidates[3]


@dataclass(frozen=True)
class ServerSettings:
    host: str
    port: int
    debug: bool
    project_root: Path
    repository_root: Path
    remote_root: Path
    database_path: Path
    minivcs_executable: Path
    frontend_origin: str
    frontend_dir: Path = PROJECT_ROOT / "frontend"
    allowed_origins: tuple = ()
    login_max_attempts: int = 10
    login_window_seconds: int = 300
    register_max_attempts: int = 20
    register_window_seconds: int = 3600


def load_settings() -> ServerSettings:
    data_dir = Path(
        os.getenv(
            "MINIVCS_DATA_DIR",
            str(PROJECT_ROOT / "server-data"),
        )
    ).resolve()

    repository_root = Path(
        os.getenv(
            "MINIVCS_REPOSITORY_ROOT",
            str(data_dir / "repositories"),
        )
    ).resolve()

    database_path = Path(
        os.getenv(
            "MINIVCS_DATABASE_PATH",
            str(data_dir / "server.db"),
        )
    ).resolve()

    remote_root = Path(
        os.getenv(
            "MINIVCS_REMOTE_ROOT",
            str(data_dir / "remotes"),
        )
    ).resolve()

    executable_value = os.getenv("MINIVCS_EXECUTABLE")

    if executable_value:
        minivcs_executable = Path(executable_value).resolve()
    else:
        minivcs_executable = _default_executable().resolve()

    frontend_origin = os.getenv(
        "MINIVCS_FRONTEND_ORIGIN",
        "http://127.0.0.1:5500",
    )

    # Comma-separated list of extra browser origins allowed to call the API.
    # Not needed when the frontend is served by this server (same origin).
    origin_values = [
        value.strip()
        for value in os.getenv(
            "MINIVCS_ALLOWED_ORIGINS",
            frontend_origin,
        ).split(",")
        if value.strip()
    ]

    frontend_dir = Path(
        os.getenv(
            "MINIVCS_FRONTEND_DIR",
            str(PROJECT_ROOT / "frontend"),
        )
    ).resolve()

    return ServerSettings(
        host=os.getenv("MINIVCS_HOST", "127.0.0.1"),
        # Render/Railway/Heroku provide the port in PORT.
        port=_read_int("PORT", _read_int("MINIVCS_PORT", 8000)),
        debug=_read_bool("MINIVCS_DEBUG", False),
        frontend_origin=frontend_origin,
        project_root=PROJECT_ROOT,
        repository_root=repository_root,
        database_path=database_path,
        remote_root=remote_root,
        minivcs_executable=minivcs_executable,
        frontend_dir=frontend_dir,
        allowed_origins=tuple(origin_values),
        login_max_attempts=_read_int("MINIVCS_LOGIN_MAX_ATTEMPTS", 10),
        login_window_seconds=_read_int("MINIVCS_LOGIN_WINDOW_SECONDS", 300),
        register_max_attempts=_read_int("MINIVCS_REGISTER_MAX_ATTEMPTS", 20),
        register_window_seconds=_read_int(
            "MINIVCS_REGISTER_WINDOW_SECONDS", 3600
        ),
    )
