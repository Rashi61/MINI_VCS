# MiniVCS shared interfaces

Agreed formats that every module relies on. Change them only after telling
the whole team, because several members' code reads these files.

## Metadata layout (`.minivcs/`)

| Path | Contents | Written by |
|------|----------|------------|
| `config` | `repository_format=1` | `Repository::init` |
| `HEAD` | current branch **name** only, e.g. `main` (one line) | init, branch switch |
| `branches/<name>` | ID of that branch's latest commit (one line, empty = no commits yet) | `CommitManager::createCommit`, branch code |
| `tracked.txt` | one tracked relative path per line | `FileManager` |
| `objects/<commitId>/commit.txt` | commit metadata (format below) | `createCommit` |
| `objects/<commitId>/files/<path>` | stored copy of each committed file | `createCommit` |
| `objects/<commitId>/pruned` | marker: stored files removed by retention | `RetentionManager` |
| `retention.cfg` | `retention_days=<n>` | `RetentionManager` |

## commit.txt

Each value is written with `std::quoted`, in this order:

```
"<commit id>"
"<message>"
"<YYYY-MM-DD HH:MM:SS>"
"<parent commit id, empty for the first commit>"
<number of files>
"<relative path>"
"<sha-256 of the file>"
... (one path/hash pair per file, sorted by path)
```

The commit ID is the SHA-256 of parent ID, message, a millisecond timestamp
and every `path:hash` line.

## Rules for other modules

- **Branches (Member 2):** create `branches/<name>` containing a valid commit
  ID, change the current branch only by rewriting `HEAD`, and read commits
  through `CommitManager` (`getCommit`, `getCommitHistory`,
  `getBranchTips`). Do not create a second commit store.
- **Retention (Member 4):** call `RetentionManager::setProtected(
  CommitManager::getBranchTips())` before `cleanup()` so no branch tip loses
  its files. Retention deletes only `objects/<id>/files`; never `commit.txt`.
- **Database (Member 3):** the filesystem is the source of truth for commits
  and branches. Keep users, permissions and repository records in the
  database, and refer to commits by ID only.
- **Remote (Member 4):** push and pull exchange `commit.txt` plus
  `files/`; verify each file against its stored SHA-256 on receipt.