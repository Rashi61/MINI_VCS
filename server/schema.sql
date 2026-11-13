
PRAGMA foreign_keys = ON;

-- Table for storing user accounts
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    email TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    password_salt TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- Table for storing repository details
CREATE TABLE IF NOT EXISTS repositories (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    owner_id INTEGER NOT NULL,
    visibility TEXT NOT NULL DEFAULT 'private'
        CHECK (visibility IN ('private', 'public')),
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (owner_id) REFERENCES users(id)
        ON DELETE CASCADE
);

-- Table for storing repository members and their roles
CREATE TABLE IF NOT EXISTS repository_members (
    repository_id INTEGER NOT NULL,
    user_id INTEGER NOT NULL,
    role TEXT NOT NULL
        CHECK (role IN ('owner', 'maintainer', 'contributor', 'viewer')),
    joined_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (repository_id, user_id),
    FOREIGN KEY (repository_id) REFERENCES repositories(id)
        ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(id)
        ON DELETE CASCADE
);

-- Table for storing login sessions
CREATE TABLE IF NOT EXISTS sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    token_hash TEXT NOT NULL UNIQUE,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    expires_at TEXT NOT NULL,
    revoked_at TEXT,
    FOREIGN KEY (user_id) REFERENCES users(id)
        ON DELETE CASCADE
);

-- Table for storing repository branch references
CREATE TABLE IF NOT EXISTS branch_refs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    repository_id INTEGER NOT NULL,
    branch_name TEXT NOT NULL,
    commit_hash TEXT,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (repository_id, branch_name),
    FOREIGN KEY (repository_id) REFERENCES repositories(id)
        ON DELETE CASCADE
);

-- Indexes for faster database searching
CREATE INDEX IF NOT EXISTS idx_sessions_user
ON sessions(user_id);

CREATE INDEX IF NOT EXISTS idx_members_user
ON repository_members(user_id);

CREATE INDEX IF NOT EXISTS idx_branch_repository
ON branch_refs(repository_id);
