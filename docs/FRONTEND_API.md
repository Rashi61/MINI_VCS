# MiniVCS Frontend and Server API Contract

## 1. Purpose

This document defines the HTTP API between the MiniVCS frontend and the Python server layer.

The browser communicates only with the Python HTTP API. The server validates authentication and permissions, maps repository IDs to controlled server directories, and invokes the MiniVCS backend through safe operations.

The frontend must not send arbitrary filesystem paths to the server.

## 2. Development server

Default development address:


http://127.0.0.1:8000

## 6. Commit the automated tests

After the tests pass:

```powershell
git diff --check

git add server\tests\test_api.py docs\FRONTEND_API.md

git commit -m "Add automated Flask authentication and repository API tests"

git push origin integrate/hosted-api
```

## 11. Automated API tests

Run the complete server test suite from the project root:

```powershell
python -m unittest `
  server.tests.test_auth `
  server.tests.test_permissions `
  server.tests.test_repository_service `
  server.tests.test_api `
  -v
```

The tests use temporary databases and repository storage. They do not modify the normal development repository database.

## 12. Repository details, status, and history

### Repository details

```text
GET /api/repositories/{repository_id}
```

Public repositories may be viewed without login. Private repositories require the owner or an authorized repository member.

### Repository status

```text
GET /api/repositories/{repository_id}/status
```

The server executes the fixed MiniVCS `status` command inside the controlled repository directory.

### Repository history

```text
GET /api/repositories/{repository_id}/history
```

The server executes the fixed MiniVCS `history` command inside the controlled repository directory.

The first API version returns command output as text. A later version may add structured JSON output directly to the C++ CLI.

## 13. Branch and commit endpoints

### List branches

```text
GET /api/repositories/{repository_id}/branches
```

Public repositories may expose branch listings without login. Private repositories require repository access permission.

### Create a branch

```text
POST /api/repositories/{repository_id}/branches
```

Request:

```json
{
  "name": "feature-api"
}
```

The caller must have the `create_branch` permission.

### Create a commit

```text
POST /api/repositories/{repository_id}/commit
```

Request:

```json
{
  "message": "Update repository"
}
```

The caller must have the `push` permission.

The current commit endpoint commits files already present in the controlled server-side repository. Browser file upload and staging will be implemented separately.

## 14. Current API limitations

The current API does not yet provide:

- Browser file upload.
- Browser staging or `add`.
- Merge endpoints.
- Pull-request functionality.
- Network remote hosting.
- GitHub-compatible Git protocol support.

Member 2's merge endpoint will be added only after her merge implementation is compiled and tested separately.