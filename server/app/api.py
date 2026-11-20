from flask import Flask, jsonify

from .config import load_settings


def create_app() -> Flask:
    app = Flask(__name__)

    settings = load_settings()
    app.config["MINIVCS_SETTINGS"] = settings

    @app.get("/api/health")
    def health():
        return jsonify(
            {
                "ok": True,
                "message": "MiniVCS API is running",
                "data": {
                    "service": "minivcs-api",
                    "status": "healthy",
                    "version": "0.1.0",
                },
            }
        )

    return app