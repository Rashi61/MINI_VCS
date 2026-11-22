
import os
import sqlite3
from pathlib import Path


SERVER_ROOT = Path(__file__).resolve().parents[1]
PROJECT_ROOT = SERVER_ROOT.parent

DEFAULT_DB_PATH = PROJECT_ROOT / "server-data" / "server.db"

# Retain the old constant for compatibility.
DB_PATH = DEFAULT_DB_PATH

SCHEMA_PATH = SERVER_ROOT / "schema.sql"


def configured_database_path():
    configured = os.getenv("MINIVCS_DATABASE_PATH")

    if configured:
        return Path(configured).resolve()

    return DEFAULT_DB_PATH.resolve()


def get_connection(db_path=None):
    if db_path is not None:
        database_path = Path(db_path).resolve()
    else:
        database_path = configured_database_path()

    database_path.parent.mkdir(parents=True, exist_ok=True)

    connection = sqlite3.connect(str(database_path))
    connection.row_factory = sqlite3.Row
    connection.execute("PRAGMA foreign_keys = ON")

    return connection


def ensure_repository_description_column(connection):
    columns = {
        row["name"]
        for row in connection.execute(
            "PRAGMA table_info(repositories)"
        ).fetchall()
    }

    if "description" not in columns:
        connection.execute(
            """
            ALTER TABLE repositories
            ADD COLUMN description TEXT NOT NULL DEFAULT ''
            """
        )


def initialize_database(db_path=None):
    connection = get_connection(db_path)

    try:
        with SCHEMA_PATH.open("r", encoding="utf-8") as schema_file:
            schema = schema_file.read()

        connection.executescript(schema)

        ensure_repository_description_column(connection)

        connection.commit()

    finally:
        connection.close()


def check_tables(db_path=None):
    connection = get_connection(db_path)

    try:
        tables = connection.execute(
            """
            SELECT name
            FROM sqlite_master
            WHERE type = 'table'
            ORDER BY name
            """
        ).fetchall()

        return [table["name"] for table in tables]

    finally:
        connection.close()


if __name__ == "__main__":
    initialize_database()
    print(check_tables())
