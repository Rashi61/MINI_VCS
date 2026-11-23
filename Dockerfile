# ---------- Stage 1: build the C++ backend ----------
FROM debian:bookworm-slim AS build

RUN apt-get update \
 && apt-get install -y --no-install-recommends build-essential cmake libssl-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt ./
COPY include ./include
COPY src ./src
COPY tests ./tests
COPY third_party ./third_party

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build --target minivcs -j2

# ---------- Stage 2: Python web server + frontend ----------
FROM python:3.12-slim-bookworm

RUN apt-get update \
 && apt-get install -y --no-install-recommends libssl3 \
 && rm -rf /var/lib/apt/lists/* \
 && useradd --create-home --uid 10001 minivcs \
 && mkdir -p /data \
 && chown minivcs:minivcs /data

WORKDIR /app

COPY server/requirements.txt ./server/requirements.txt
RUN pip install --no-cache-dir -r server/requirements.txt

COPY server ./server
COPY frontend ./frontend
COPY --from=build /src/build/minivcs /usr/local/bin/minivcs

ENV MINIVCS_EXECUTABLE=/usr/local/bin/minivcs \
    MINIVCS_DATA_DIR=/data \
    MINIVCS_HOST=0.0.0.0 \
    MINIVCS_TRUST_PROXY=true \
    PYTHONUNBUFFERED=1

USER minivcs
EXPOSE 8000

HEALTHCHECK --interval=30s --timeout=5s --start-period=20s --retries=3 \
  CMD python -c "import os,urllib.request;urllib.request.urlopen('http://127.0.0.1:%s/api/health' % os.environ.get('PORT','8000'),timeout=4)" || exit 1

# One worker on purpose: login rate limiting and per-repository locks are
# in-memory. Threads give concurrency; see DEPLOY.md before scaling out.
CMD ["sh", "-c", "exec gunicorn server.run:app --bind 0.0.0.0:${PORT:-8000} --workers 1 --threads 8 --timeout 120 --access-logfile -"]
