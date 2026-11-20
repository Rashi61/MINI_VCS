from .app.api import create_app
from .app.config import load_settings


app = create_app()


if __name__ == "__main__":
    settings = load_settings()

    app.run(
        host=settings.host,
        port=settings.port,
        debug=settings.debug,
    )