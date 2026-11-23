"""Create demo accounts and repositories on a running MiniVCS server.

Usage (server must already be running):
    python scripts/seed_demo.py                    # http://127.0.0.1:8000
    python scripts/seed_demo.py https://my-app.onrender.com

Safe to run more than once: existing accounts/repositories are reused.
Only the Python standard library is needed.
"""
import json
import sys
import urllib.error
import urllib.request
import uuid

BASE = (sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:8000").rstrip("/")
PASSWORD = "Demo@12345"
USERS = ["demo_owner", "demo_dev"]


def call(method, path, token=None, payload=None, multipart=None):
    headers = {}
    data = None

    if token:
        headers["Authorization"] = f"Bearer {token}"

    if payload is not None:
        data = json.dumps(payload).encode()
        headers["Content-Type"] = "application/json"
    elif multipart is not None:
        boundary = uuid.uuid4().hex
        parts = []
        for name, value in multipart["fields"].items():
            parts.append(
                f'--{boundary}\r\nContent-Disposition: form-data; name="{name}"'
                f"\r\n\r\n{value}\r\n"
            )
        body = "".join(parts).encode()
        body += (
            f'--{boundary}\r\nContent-Disposition: form-data; name="file"; '
            f'filename="upload.txt"\r\nContent-Type: text/plain\r\n\r\n'
        ).encode()
        body += multipart["content"].encode() + f"\r\n--{boundary}--\r\n".encode()
        data = body
        headers["Content-Type"] = f"multipart/form-data; boundary={boundary}"

    request = urllib.request.Request(BASE + path, data=data, headers=headers, method=method)

    try:
        with urllib.request.urlopen(request, timeout=60) as response:
            return response.status, json.loads(response.read() or b"{}")
    except urllib.error.HTTPError as error:
        try:
            body = json.loads(error.read() or b"{}")
        except ValueError:
            body = {}
        return error.code, body


def must(status_body, what):
    status, body = status_body
    if status >= 400:
        raise SystemExit(f"FAILED: {what}: HTTP {status} {body.get('message', body)}")
    return body.get("data", {})


def login(username):
    call("POST", "/api/auth/register", payload={
        "username": username,
        "email": f"{username}@example.com",
        "password": PASSWORD,
    })
    data = must(call("POST", "/api/auth/login", payload={
        "username": username, "password": PASSWORD}), f"login {username}")
    return data["token"]


def make_repo(token, name, description, visibility):
    status, body = call("POST", "/api/repositories", token, {
        "name": name, "description": description, "visibility": visibility})
    if status == 201:
        print(f"  created repository '{name}' ({visibility})")
        return body["data"]["id"], True

    status, body = call("GET", "/api/repositories", token)
    for repo in body.get("data", {}).get("repositories", []):
        if repo["name"] == name:
            print(f"  repository '{name}' already exists - skipped")
            return repo["id"], False

    raise SystemExit(f"FAILED: could not create or find repository {name}")


def put(token, repo, path, text, message):
    must(call("POST", f"/api/repositories/{repo}/files", token,
              multipart={"fields": {"path": path}, "content": text}), f"upload {path}")
    must(call("POST", f"/api/repositories/{repo}/commit", token,
              payload={"message": message}), f"commit '{message}'")
    print(f"    commit: {message}")


def branch(token, repo, name):
    must(call("POST", f"/api/repositories/{repo}/branches", token, {"name": name}), f"branch {name}")


def switch(token, repo, name):
    must(call("POST", f"/api/repositories/{repo}/branches/switch", token, {"name": name}), f"switch {name}")


def main():
    print(f"Seeding demo data on {BASE}")
    tokens = {name: login(name) for name in USERS}
    owner = tokens["demo_owner"]
    print(f"  accounts ready: {', '.join(USERS)}  (password: {PASSWORD})")

    # 1. Public project with history, a branch and a completed merge
    repo, fresh = make_repo(owner, "demo-project",
        "Sample project showing commits, branches and a merge.", "public")
    if fresh:
        put(owner, repo, "docs/readme.txt",
            "Demo Project\n============\nA small sample used to demonstrate MiniVCS.\n",
            "Initial commit: add README")
        put(owner, repo, "src/app.py", "def main():\n    print('Hello, MiniVCS!')\n",
            "Add application entry point")
        branch(owner, repo, "feature-login")
        switch(owner, repo, "feature-login")
        put(owner, repo, "src/login.py",
            "def login(user, password):\n    return user == 'demo'\n",
            "Add login module")
        switch(owner, repo, "main")
        put(owner, repo, "docs/changelog.txt", "v0.1 - first release\n",
            "Add changelog")
        must(call("POST", f"/api/repositories/{repo}/merge", owner,
                  {"source": "feature-login"}), "merge feature-login")
        print("    merged branch 'feature-login' into main")

    # 2. Public repo left ready for a LIVE merge-conflict demo
    repo, fresh = make_repo(owner, "conflict-demo",
        "Two branches edit the same file: merge them live to see conflict markers.", "public")
    if fresh:
        put(owner, repo, "notes.txt", "Shared notes\nline: original\n", "Add shared notes")
        branch(owner, repo, "other-branch")
        switch(owner, repo, "other-branch")
        put(owner, repo, "notes.txt", "Shared notes\nline: edited on other-branch\n",
            "Edit notes on other-branch")
        switch(owner, repo, "main")
        put(owner, repo, "notes.txt", "Shared notes\nline: edited on main\n",
            "Edit notes on main")
        print("    NOT merged on purpose: merge 'other-branch' in the UI to show a conflict")

    # 3. Public repo ready for a LIVE clean merge
    repo, fresh = make_repo(owner, "merge-demo",
        "A feature branch that merges cleanly: try the Merge button.", "public")
    if fresh:
        put(owner, repo, "readme.txt", "Merge demo\n", "Initial commit")
        branch(owner, repo, "new-feature")
        switch(owner, repo, "new-feature")
        put(owner, repo, "feature.txt", "A brand new feature\n", "Add new feature")
        switch(owner, repo, "main")
        print("    NOT merged on purpose: merge 'new-feature' in the UI")

    # 4. Private repo owned by the second user
    dev = tokens["demo_dev"]
    repo, fresh = make_repo(dev, "private-notes", "A private repository.", "private")
    if fresh:
        put(dev, repo, "todo.txt", "1. Prepare presentation\n", "Add todo list")

    print("\nDone. Log in with demo_owner / " + PASSWORD)


if __name__ == "__main__":
    main()
