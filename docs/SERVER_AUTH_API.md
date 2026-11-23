# MiniVCS — Server Authentication and Permissions API

## 1. Overview

The MiniVCS server module provides user authentication, secure session management and repository-level access control.

It uses Python and SQLite to manage user accounts, login sessions, repositories and member roles.

## 2. Technologies Used

- Python 3
- SQLite database
- PBKDF2-HMAC-SHA256 for password hashing
- SHA-256 for session token hashing
- Python hashlib, secrets and sqlite3 libraries

## 3. Database Structure

The server database contains five main tables:

| Table | Purpose |
|---|---|
| users | Stores user account details |
| repositories | Stores repository information |
| repository_members | Stores repository members and roles |
| sessions | Stores login session information |
| branch_refs | Stores branch references and commit hashes |

## 4. Authentication Module

File: `server/app/auth.py`

### User Registration

Function: `register_user(username, email, password)`

- Registers a new user.
- Checks required fields.
- Requires a minimum password length of 8 characters.
- Hashes the password before storing it.
- Rejects duplicate usernames and email addresses.
- Returns the registered user's ID.

### User Login

Function: `login_user(username, password)`

- Checks the username and password.
- Generates a secure session token.
- Stores the hashed token in the database.
- Creates a session with a 24-hour expiry.
- Returns the user ID, username and session token.

### Session Verification

Function: `verify_session(token)`

- Checks whether the session exists.
- Rejects expired or revoked sessions.
- Returns the user ID if the session is valid.
- Returns None if the session is invalid.

### User Logout

Function: `logout_user(token)`

- Finds the session using its token hash.
- Revokes the session.
- Returns True if logout succeeds, otherwise False.

## 5. Password Security

Passwords are secured using PBKDF2-HMAC-SHA256.

Configuration:

- Salt size: 16 bytes
- Hash size: 32 bytes
- Iterations: 310000

Random salts are generated for passwords. Plain-text passwords are not stored in the database.

Session tokens are also stored as SHA-256 hashes rather than plain-text tokens.

## 6. Repository Permissions

File: `server/app/permissions.py`

MiniVCS supports four repository roles:

| Role | Permissions |
|---|---|
| Owner | Full repository control |
| Maintainer | Manage members and repository settings |
| Contributor | Push code and create branches |
| Viewer | View, clone and pull |

### Permission Functions

**has_permission(role, action)**

Checks whether a particular role is allowed to perform an action.

**get_repository_role(user_id, repository_id)**

Finds the user's role in a repository by checking its owner and membership records.

**check_repository_permission(user_id, repository_id, action)**

Checks whether a user has permission to perform a specific repository action.

## 7. Automated Testing

Authentication tests:

File: `server/tests/test_auth.py`

- User registration
- Duplicate registration
- Successful login
- Incorrect password
- Session verification and logout

Permission tests:

File: `server/tests/test_permissions.py`

- Owner permissions
- Maintainer permissions
- Contributor permissions
- Viewer permissions
- Invalid roles
- Database-based permission checking

### Running Tests

Run authentication tests:

```bash
python -m unittest server.tests.test_auth -v
```

Run permission tests:

```bash
python -m unittest server.tests.test_permissions -v
```

## 8. Current Implementation Status

The authentication and permission modules have been implemented and tested independently using temporary SQLite databases.

- Authentication tests: 5 passed
- Permission tests: 6 passed
- Total: 11 tests passed

## 9. Future Integration

The Python server module can be integrated with the existing C++ MiniVCS client through an agreed API.

Future work can include HTTP endpoints, client-server communication and integration with repository operations.