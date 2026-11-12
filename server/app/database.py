import sqlite3
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent.parent
DB_PATH = BASE_DIR / "data" / "minivcs_server.db"
SCHEMA_PATH = BASE_DIR / "schema.sql"


def get_connection(db_path=DB_PATH):
    Path(db_path).parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA foreign_keys = ON")
    return conn


def initialize_database(db_path=DB_PATH):
    conn = get_connection(db_path)

    try:
        with open(SCHEMA_PATH, "r", encoding="utf-8") as file:
            schema = file.read()

        conn.executescript(schema)
        conn.commit()

    finally:
        conn.close()

def check_tables():
    conn = get_connection()

    try:
        tables = conn.execute(
            "SELECT name FROM sqlite_master WHERE type='table'"
        ).fetchall()

        for table in tables:
            print(table["name"])

    finally:
        conn.close()


if __name__ == "__main__":
    check_tables()