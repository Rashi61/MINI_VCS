import logging

from .app.api import create_app
from .app.config import load_settings


app = create_app()


def main():
    settings = load_settings()

    if settings.debug:
        # Flask development server (auto-reload). Never use in production.
        app.run(
            host=settings.host,
            port=settings.port,
            debug=True,
        )
        return

    try:
        from waitress import serve
    except ImportError:
        logging.warning(
            "waitress is not installed; falling back to Flask's server. "
            "Run: pip install -r server/requirements.txt"
        )
        app.run(
            host=settings.host,
            port=settings.port,
            debug=False,
        )
        return

    # Production-grade server that also works on Windows.
    serve(
        app,
        host=settings.host,
        port=settings.port,
        threads=8,
    )


if __name__ == "__main__":
    main()
