# MiniVCS

MiniVCS is a lightweight version-control system developed as a B.Tech DSCPP-III project. It is designed to demonstrate the core ideas behind repository management, file tracking, commits, snapshots, restoration, branches, metadata storage, authentication, permissions, and future remote-repository support.

The project currently consists of a C++ command-line backend, a local file-system storage layer, SQLite metadata/authentication support, and an initial frontend/remote-repository structure.

## 1. Current Phase-II Status

The Phase-II integration branch combines the core backend, branch-management functionality, and the database/authentication module.

### Implemented and validated

- Repository initialization.
- Repository-root discovery from nested directories.
- File tracking with `add`.
- File removal from tracking with `remove`.
- Tracked-file listing.
- SHA-256 file hashing using OpenSSL.
- Commit creation with commit messages.
- Commit snapshots stored inside `.minivcs`.
- Parent-commit links.
- Commit history traversal.
- Status reporting for modified, missing, untracked, and staged files.
- Restoration of previous commit snapshots.
- Safe restore that refuses to overwrite local edits unless `--force` is used.
- `.minivcsignore` and built-in ignore rules.
- Detection and reporting of damaged commit or branch history.
- Retention configuration and cleanup of expired snapshots.
- Branch creation, listing, switching, and deletion.
- Branch pointer updates and checkout restoration.
- SQLite database initialization.
- User registration and login.
- Password hashing and verification using the Windows CNG/BCrypt API.
- Repository metadata and collaborator-role operations in the database module.
- CMake-based Windows build using MSVC, vcpkg, OpenSSL, and SQLite.

### Manually tested during Phase II

- Clean repository initialization and repeated initialization.
- Adding files and committing snapshots.
- Running commands from a nested repository directory.
- Detecting file edits and missing files.
- Restoring an earlier commit.
- Force restoration while preserving the branch pointer.
- Branch creation and listing.
- Switching between `main` and a feature branch.
- Deleting a feature branch.
- User registration.
- Login with a correct password.
- Rejection of an incorrect password.
- Logout.
- Storage of the authentication database inside the repository `.minivcs` directory.

### Not yet complete

- Full merge implementation with automatic three-way merge and conflict markers.
- Complete frontend-to-backend integration.
- Remote repository operations and synchronization.
- Cross-platform authentication implementation for macOS and Linux.
- Complete automated tests for branch management and database/authentication operations.
- Final end-to-end testing across all team modules and operating systems.

The project should not claim that merge, remote repository support, frontend-backend integration, or cross-platform authentication are complete until they have been implemented and tested.

## 2. Technology Stack

### Backend

- C++17.
- CMake 3.20 or later.
- Object-oriented backend modules.
- Windows MSVC or another C++17-compatible compiler.

### Security and hashing

- OpenSSL Crypto for SHA-256 file hashing.
- Windows CNG/BCrypt PBKDF2-HMAC-SHA256 for the current Windows authentication implementation.

### Database

- SQLite amalgamation source included under `third_party/sqlite3`.
- SQLite stores user, repository metadata, and collaborator information.
- Versioned project files and snapshots remain stored on the local file system.

### Frontend and remote layer

- Initial HTML/CSS/JavaScript frontend structure.
- Remote-repository files are under development and are not yet part of the complete backend workflow.

## 3. Repository Structure

```text
MiniVCS/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── Commit.h
│   ├── CommitManager.h
│   ├── DiffManager.h
│   ├── FileManager.h
│   ├── HashManager.h
│   ├── RetentionManager.h
│   ├── member3.h
│   └── repository.h
├── src/
│   ├── main.cpp
│   ├── member3.cpp
│   ├── RetentionManager.cpp
│   ├── repository/
│   │   └── repository.cpp
│   ├── security/
│   │   └── HashManager.cpp
│   └── vcs/
│       ├── CommitManager.cpp
│       ├── DiffManager.cpp
│       ├── FileManager.cpp
│       ├── IgnoreMatcher.cpp
│       ├── branch.cpp
│       └── commit.cpp
├── tests/
│   ├── run_tests.py
│   ├── member1_regressions.py
│   └── member3_demo.cpp
├── third_party/
│   └── sqlite3/
│       ├── sqlite3.c
│       └── sqlite3.h
├── frontend/
└── docs/
```

A MiniVCS repository created by the program contains internal metadata similar to:

```text
project/
├── .minivcs/
│   ├── HEAD
│   ├── branches/
│   ├── commits/
│   ├── objects/
│   ├── snapshots/
│   └── minivcs_users.db
└── project files
```

## 4. Building on Windows

The current integrated backend was validated on Windows using Visual Studio Build Tools, MSVC, CMake, vcpkg, OpenSSL, and the SQLite amalgamation source.

Install or enable the following components:

- Desktop development with C++.
- MSVC x64/x86 build tools.
- Windows SDK.
- C++ CMake tools for Windows.
- CMake.
- Python 3.

Install OpenSSL through vcpkg:

```powershell
C:\vcpkg\vcpkg.exe install openssl:x64-windows
```

Configure the project:

```powershell
cd C:\Users\avnis\Documents\MiniVCS

Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue

cmake -S . -B build `
  -G "Visual Studio 17 2022" `
  -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows
```

The equivalent one-line command is:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows
```

Build:

```powershell
cmake --build build --config Debug
```

The executable is created at:

```text
build\Debug\minivcs.exe
```

## 5. Running the Automated Tests

Run the registered CTest tests:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

Run the main command-line test suite directly:

```powershell
python tests\run_tests.py .\build\Debug\minivcs.exe
```

Run the Member 1 regression tests directly:

```powershell
python tests\member1_regressions.py .\build\Debug\minivcs.exe
```

The Phase-II Windows validation performed so far reported:

```text
CTest: 2/2 tests passed
Main CLI suite: 99 passed, 0 failed, 5 skipped
Member 1 regression suite: all tests passed
```

The skipped cases represent branch, merge, and SQLite/authentication coverage that was not yet fully converted into automated tests. Manual branch and authentication checks were performed separately.

## 6. Command Reference

### Display help

```powershell
minivcs
```

### Initialize a repository

```powershell
minivcs init
```

### Add a file

```powershell
minivcs add <file>
```

### Remove a tracked file

```powershell
minivcs remove <file>
```

### List tracked files

```powershell
minivcs tracked
```

### Calculate a file hash

```powershell
minivcs hash <file>
```

### Create a commit

```powershell
minivcs commit "commit message"
```

### View commit history

```powershell
minivcs history
```

### View repository status

```powershell
minivcs status
```

### Restore a commit

```powershell
minivcs restore <commit-id-or-prefix>
```

To force restoration over local edits:

```powershell
minivcs restore <commit-id-or-prefix> --force
```

### Display commit details

```powershell
minivcs show <commit-id-or-prefix>
```

### Compare versions

```powershell
minivcs diff
minivcs diff <commit-id-or-prefix>
minivcs diff <older-commit> <newer-commit>
```

### Manage retention

```powershell
minivcs retention status
minivcs retention set <days>
minivcs retention cleanup
minivcs retention cleanup --dry-run
minivcs retention log
```

### Manage branches

```powershell
minivcs branch create <name>
minivcs branches
minivcs switch <name>
minivcs switch <name> --force
minivcs delete-branch <name>
```

### Authentication and metadata menu

Authentication is currently exposed through an interactive menu:

```powershell
minivcs auth
```

The menu currently supports registration, login, logout, login-status checks, repository metadata creation/listing, and collaborator operations.

The database is stored in:

```text
<repository-root>/.minivcs/minivcs_users.db
```

## 7. Design Notes

### Repository discovery

Commands executed inside a repository subdirectory search upward for the nearest initialized repository root. This allows commands such as `status`, `add`, `commit`, and `auth` to work from nested directories.

### File storage

The working files remain in the project directory. MiniVCS stores commit metadata, branch pointers, and snapshots under `.minivcs`.

### Commit history

Each commit stores its metadata and parent reference. Corrupt or unreadable history is reported as an error rather than being silently treated as an empty repository.

### Ignore rules

MiniVCS applies built-in ignored directories and entries from `.minivcsignore`. Ignored paths are not added to tracking.

### Branches

Branches are represented by pointers under `.minivcs/branches`. Switching branches restores the selected commit snapshot without changing the commit history or silently overwriting local edits unless the force option is supplied.

### Database and file-system separation

SQLite stores structured metadata such as users, repository records, and collaborator roles. Commit objects and file snapshots remain in the `.minivcs` file-system storage. The two layers still require further end-to-end consistency checks before the system can be considered complete.

## 8. Known Limitations

- Merge and conflict resolution are not complete.
- The current authentication password-hashing implementation uses Windows CNG/BCrypt and has been validated on Windows. macOS/Linux authentication requires a portable implementation or separate platform backend.
- Branch and authentication tests are currently partly manual and should be converted to automated CTest coverage.
- The frontend is not yet connected to the complete C++ command-line backend.
- Remote repository operations are under development.
- The database metadata and file-system repository state do not yet have complete transactional synchronization.
- The current Phase-II branch should be treated as an integrated development branch until it has been reviewed and merged into `main`.

## 9. Team Contributions

### Member 1: Core backend and integration

- Repository initialization and repository-root discovery.
- File tracking and ignore handling.
- Commit and snapshot management.
- Status, history, restoration, and retention functionality.
- Corrupt-history reporting.
- Branch pointer and checkout APIs.
- CMake/build integration.
- Cross-member integration and Windows validation.

### Member 2: Branch management

- Branch creation.
- Branch listing.
- Branch switching and checkout integration.
- Branch deletion.
- Branch pointer behavior and integration with the core commit manager.
- Further automated branch testing and merge/conflict implementation remain.

### Member 3: Database and authentication

- SQLite amalgamation integration.
- Database schema initialization.
- User registration and login.
- Password hashing and verification.
- Repository metadata and collaborator-role operations.
- Further automated tests and cross-platform authentication remain.

### Member 4: Frontend and remote-repository work

- Initial frontend structure and retention-related frontend files.
- Remote-repository work remains to be integrated and tested with the backend.

## 10. Phase-II Completion Plan

Before declaring the project fully complete, the team should:

1. Add automated branch-management tests.
2. Add automated SQLite/authentication tests.
3. Implement and test merge behavior with conflict detection.
4. Integrate and test the frontend with the backend.
5. Complete remote-repository functionality.
6. Decide on and implement cross-platform authentication.
7. Validate database/file-system consistency.
8. Run a clean end-to-end workflow on Windows and macOS.
9. Update the final report with actual commands, outputs, limitations, and screenshots.
10. Merge the reviewed Phase-II integration branch into `main` only after the evaluator checkpoint is complete.

## 11. Demonstration Workflow

A concise backend demonstration can use the following sequence:

```powershell
minivcs init
"first version" | Set-Content file.txt
minivcs add file.txt
minivcs commit "initial commit"
minivcs status
minivcs history
minivcs branch create feature
minivcs switch feature
minivcs branches
minivcs switch main
minivcs auth
```

For the evaluator, explain clearly which features are implemented and tested, which were manually demonstrated, and which remain planned. This distinction is part of the current project status and should not be hidden.
