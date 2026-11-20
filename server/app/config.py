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


@dataclass(frozen=True)
class ServerSettings:
    host: str
    port: int
    debug: bool
    project_root: Path
    repository_root: Path
    database_path: Path
    minivcs_executable: Path


def load_settings() -> ServerSettings:
    repository_root = Path(
        os.getenv(
            "MINIVCS_REPOSITORY_ROOT",
            str(PROJECT_ROOT / "server-data" / "repositories"),
        )
    ).resolve()

    database_path = Path(
        os.getenv(
            "MINIVCS_DATABASE_PATH",
            str(PROJECT_ROOT / "server-data" / "server.db"),
        )
    ).resolve()

    executable_default = (
        PROJECT_ROOT / "build" / "Debug" / "minivcs.exe"
    )

    minivcs_executable = Path(
        os.getenv(
            "MINIVCS_EXECUTABLE",
            str(executable_default),
        )
    ).resolve()

    return ServerSettings(
        host=os.getenv("MINIVCS_HOST", "127.0.0.1"),
        port=int(os.getenv("MINIVCS_PORT", "8000")),
        debug=_read_bool("MINIVCS_DEBUG", False),
        project_root=PROJECT_ROOT,
        repository_root=repository_root,
        database_path=database_path,
        minivcs_executable=minivcs_executable,
    )