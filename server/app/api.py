from flask import Flask, jsonify

from .config import load_settings
from .database import check_tables


def create_app() -> Flask:
    app = Flask(__name__)

    settings = load_settings()

    app.config["MINIVCS_SETTINGS"] = settings

    @app.get("/api/health")
    def health():
        try:
            tables = check_tables()

            return jsonify(
                {
                    "ok": True,
                    "message": "MiniVCS API is running",
                    "data": {
                        "service": "minivcs-api",
                        "status": "healthy",
                        "version": "0.1.0",
                        "database": "ready",
                        "tables": tables,
                    },
                }
            )
        except Exception:
            return jsonify(
                {
                    "ok": False,
                    "message": "MiniVCS API database is unavailable",
                    "error_code": "DATABASE_UNAVAILABLE",
                }
            ), 503

    return app