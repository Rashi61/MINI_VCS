# MiniVCS: Mini Version Control System with Remote Repository and Smart Version Retention

MiniVCS is a mini version control system designed to help users manage project files, track changes, maintain commit history, and recover previous versions of their work. Inspired by the basic concepts of Git, the project aims to provide a simple system for managing repositories, commits, branches, and file versions through a web-based interface.

The project is being developed as part of the B.Tech DSCPP-III (2026) project.

---

## 1. Project Overview

Managing multiple versions of project files manually can become difficult when files are updated frequently. Keeping separate copies makes it harder to identify the latest version, track changes, and restore an earlier state.

MiniVCS aims to simplify this process by providing a centralized way to organize project files and maintain their version history.

The system is designed to support repository creation, file management, commits, commit history, snapshots, version restoration, branching, merging, user authentication, access permissions, and remote-repository functionality.

The project combines a C++ backend, a web-based frontend, and an SQLite database. The backend handles version-control operations, the frontend provides the user interface, and the database stores metadata related to users, repositories, and commits.

**Current development status:** The initial project structure has been established. Basic repository creation, file addition, and commit operations have been implemented at an initial level. The SQLite schema and basic database operations are available, and the initial frontend layout has been prepared. Several features remain under development and require integration and testing.

---

## 2. Project Objectives

The main objectives of MiniVCS are:

- To create and manage repositories for organizing project files.
- To allow users to add files and track their changes.
- To record changes through commits with relevant metadata.
- To maintain commit history for tracking project development.
- To create snapshots of files so that earlier versions can be recovered.
- To support branches for developing different versions of a project independently.
- To support merging changes from different branches.
- To identify file modifications using SHA-256 hashing.
- To provide user registration, login, and access permissions.
- To develop a web-based interface for interacting with version-control features.
- To explore remote-repository support for managing repositories across different locations.
- To maintain consistency between database records and files stored on disk.

---

## 3. Features and Development Status

The following table describes the current implementation status of the major features.

| Feature | Description | Status |
|---|---|---|
| Repository Creation | Create repositories for organizing project files. | Implemented at a basic level |
| File Management | Add files to a repository. | Basic implementation available |
| Commit Management | Record file changes through commits. | Basic implementation available |
| Database Schema | Store metadata using SQLite tables. | Initial schema created |
| Database Operations | Perform basic insert and read operations. | Basic operations tested |
| User Registration | Store user registration information. | Initial implementation available |
| User Login | Implement basic login functionality. | Initial implementation available |
| File Hashing | Use SHA-256 to identify changes in file contents. | Part of the design; further validation required |
| Commit History | Display the history of repository changes. | Further implementation and testing required |
| Snapshot Management | Preserve versions of files. | In progress |
| File Restoration | Restore files to an earlier version. | Pending completion and testing |
| Branch Management | Maintain separate development branches. | In progress |
| Branch Merging | Combine changes from different branches. | In progress |
| Access Permissions | Restrict access based on user permissions. | Further implementation required |
| Web Interface | Provide screens for repositories, commits, branches, and users. | Initial layout prepared |
| Frontend-Backend Integration | Connect interface controls to backend operations. | Pending |
| Remote Repository | Support remote-repository operations. | Under development |
| End-to-End Testing | Test the complete application workflow. | Pending |

*Note: Features listed as in progress or pending should not be considered fully functional until their implementation and testing are complete.*

---

## 4. Technology Stack

MiniVCS uses the following technologies.

### Backend
- **C++:** Implements repository management, file handling, commit operations, and other version-control functionality.
- **Object-Oriented Programming:** Organizes the backend into classes with separate responsibilities.

### Frontend
- **HTML:** Defines the structure of the web interface.
- **CSS:** Handles the layout and visual styling.
- **JavaScript:** Intended to provide interactive frontend functionality and support communication with the backend.

### Database
- **SQLite:** Stores metadata associated with users, repositories, commits, and other project entities.

### Storage
- **Local File System:** Stores project files and saved file versions.
- **SHA-256 Hashing:** Helps identify whether file contents have changed by comparing their hashes.

### Development Tools
- **Git:** Tracks source-code changes and supports collaboration.
- **GitHub:** Hosts the project repository and its development branches.
- **C++ Compiler:** Required to compile the backend.
- **SQLite:** Used for database storage and operations.
- **Web Browser:** Used to view and test the frontend.

---

## 5. System Architecture

MiniVCS is organized into three main components: the frontend, the version-control backend, and the storage layer.

### 5.1 Frontend Layer

The frontend is developed using HTML, CSS, and JavaScript.

It provides the interface through which users are intended to interact with the system.

The planned interface includes:

- Repository management screens.
- Commit history views.
- Branch management screens.
- User registration and login screens.
- Controls for performing version-control operations.

The initial frontend layout has been prepared. However, the interface is not yet connected to the C++ backend, so the controls cannot currently execute the complete version-control workflow.

### 5.2 Backend Layer

The backend is implemented in C++ and is responsible for performing version-control operations.

The backend is organized into separate classes to improve modularity and make development and testing easier.

The planned responsibilities of the main classes are described below.

| Component | Responsibility |
|---|---|
| Repository | Handles repository-related operations. |
| FileManager | Manages project files and file-related operations. |
| Commit | Represents a commit and its associated information. |
| CommitManager | Handles commit creation and commit-related operations. |
| HashManager | Calculates and compares SHA-256 hashes to identify changes. |
| BranchManager | Intended to manage branches and branch-related operations. |

The core repository and commit functionality has been implemented at an initial level. Branching, merging, snapshot management, and other operations require additional development and validation.

### 5.3 Storage Layer

MiniVCS uses SQLite for metadata storage and the local file system for actual project files and saved versions.

The database is intended to maintain information such as:

- User information.
- Repository details.
- Commit metadata.
- Relationships between repositories and their commits.
- Other information required by the implemented modules.

The database schema currently contains five tables.

The actual project files and saved versions are stored on disk rather than directly in the database.

Maintaining consistency between the database and the file system is an important requirement. For example, a commit should not be treated as successfully completed if its required snapshot has not been saved correctly.

---

## 6. Working Principle

The intended MiniVCS workflow is described below.

### Step 1: Repository Creation

A user creates a repository to organize project files.

The repository-related information is stored using the backend and the database.

### Step 2: Adding Files

The user adds files to the repository.

The backend manages the file-related operations and prepares the files for version tracking.

### Step 3: Detecting Changes

SHA-256 hashing is intended to help identify modifications to tracked files.

The hash of a file can be compared with the hash of its previously saved version.

If the hashes differ, the file contents have changed.

### Step 4: Creating Commits

A commit represents a recorded state of the project.

The intended commit metadata includes:

- Commit ID.
- Commit message.
- Date and time.
- Author information.
- Reference to the previous commit, where applicable.

The commit-related functionality has an initial implementation, while snapshot handling and complete version-history support require further work.

### Step 5: Maintaining Version History

The system is intended to maintain a sequence of commits so users can track changes over time.

Each commit should be associated with the appropriate repository and saved version information.

### Step 6: Restoring Previous Versions

The planned restoration feature will allow users to recover an earlier saved state of their files.

This requires reliable snapshot storage, commit-history tracking, and restoration logic.

The complete restoration workflow has not yet been validated.

### Step 7: Branching and Merging

Branches are intended to allow different versions of a project to be developed independently.

The merging feature will combine changes from different branches.

If conflicting changes are detected, the system should identify the conflict instead of silently overwriting one version with another.

Branching and merging are still under development.

### Step 8: User Access and Remote Repositories

The project also aims to support user authentication, access permissions, and remote-repository functionality.

These features require additional implementation and testing before they can be considered complete.

---

## 7. Database Design

SQLite is used to store project metadata.

The database schema has been created with five tables. Basic insert and read operations have been implemented and tested.

The database is intended to support operations related to users, repositories, commits, and the relationships between them.

The exact responsibilities and relationships of the tables should be documented alongside the final schema.

### Database Responsibilities

- Store user-related information.
- Maintain repository metadata.
- Store commit-related information.
- Support retrieval of stored records.
- Maintain relationships required by the backend.
- Support authentication and access-control operations as they are completed.

### Data Storage Approach

MiniVCS separates metadata storage from file storage.

- **SQLite database:** Stores structured metadata.
- **Local file system:** Stores actual project files and saved versions.

Further testing is required to verify that database records remain consistent with the corresponding files and snapshots.

---

## 8. Project Structure

The project is organized into separate modules to support independent development and testing.

The following is the intended high-level structure. The exact directory and file names should be updated to match the actual GitHub repository.

```text
MINI_VCS/
│
├── README.md
├── CMakeLists.txt
│
├── src/
│   ├── Repository.cpp
│   ├── FileManager.cpp
│   ├── Commit.cpp
│   ├── CommitManager.cpp
│   ├── HashManager.cpp
│   └── BranchManager.cpp
│
├── include/
│   ├── Repository.h
│   ├── FileManager.h
│   ├── Commit.h
│   ├── CommitManager.h
│   ├── HashManager.h
│   └── BranchManager.h
│
├── frontend/
│   ├── index.html
│   ├── css/
│   ├── js/
│   └── assets/
│
├── database/
│   └── schema.sql
│
├── repositories/
│
├── tests/
│
└── docs/
