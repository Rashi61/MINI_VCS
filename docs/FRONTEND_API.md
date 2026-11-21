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