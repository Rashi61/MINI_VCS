# MiniVCS: run locally on Windows and deploy online

## A. Run on Windows (PowerShell, from the project folder)

```powershell
# 1. Build the C++ backend (once)
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake --build build --config Debug

# 2. Python environment (once)
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r server\requirements.txt

# 3. Start the website (frontend + API on one address)
python -m server.run
```

Open http://127.0.0.1:8000. No second frontend server is needed.
Use `$env:MINIVCS_DEBUG="true"` before step 3 for auto-reload while developing.

### Run all tests

```powershell
ctest --test-dir build -C Debug --output-on-failure
python -m unittest server.tests.test_auth server.tests.test_permissions server.tests.test_repository_service server.tests.test_api server.tests.test_extras -v
```

## B. Deploy online (Render)

1. Push this branch to GitHub and merge it into `main`.
2. Render dashboard: **New > Blueprint**, select the repository (it reads `render.yaml`).
3. Wait for the Docker build (about 5-10 minutes). Your site is at `https://<name>.onrender.com`.
4. Check `https://<name>.onrender.com/api/health`.

Railway: **New Project > Deploy from GitHub repo**. It detects the `Dockerfile`.
Add variables `MINIVCS_TRUST_PROXY=true` and `MINIVCS_DATA_DIR=/data`, and attach a volume at `/data`.

## C. Important limits

- **Free Render disk is ephemeral.** Accounts, repositories and commits are wiped on every restart,
  redeploy, and after idle spin-down. Fine for a demo; for real use attach a persistent disk
  (paid Render plan, or a Railway volume) mounted at `/data`.
- Run **one** server worker (already set). Login rate limiting and repository locks are in-memory.
- Each repository has one shared working tree on the server, so two people switching branches
  in the same repository at the same moment affect each other.
- "Remote" clone/push/pull copies between folders on the server; it is not a network Git protocol.
- Sessions are bearer tokens kept in the browser. Always use the HTTPS address in production.

## D. Environment variables

| Variable | Default | Purpose |
|---|---|---|
| `PORT` | 8000 | Port (set by Render/Railway) |
| `MINIVCS_HOST` | 127.0.0.1 | Bind address (`0.0.0.0` in Docker) |
| `MINIVCS_DATA_DIR` | `server-data` | Database, repositories and remotes |
| `MINIVCS_EXECUTABLE` | auto-detected | Path to the `minivcs` binary |
| `MINIVCS_TRUST_PROXY` | false | Set true behind Render/Railway |
| `MINIVCS_ALLOWED_ORIGINS` | `http://127.0.0.1:5500` | Extra CORS origins (not needed when same-origin) |
| `MINIVCS_LOGIN_MAX_ATTEMPTS` / `_WINDOW_SECONDS` | 10 / 300 | Failed-login limit |
| `MINIVCS_REGISTER_MAX_ATTEMPTS` / `_WINDOW_SECONDS` | 20 / 3600 | Registration limit per IP |
| `MINIVCS_DEBUG` | false | Flask dev server (never in production) |
