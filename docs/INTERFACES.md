MiniVCS Shared Interfaces
This document defines the shared filesystem layout and interfaces used by all MiniVCS modules.

Changes to the commit format, branch format, snapshot format, or repository metadata must be discussed with all members before implementation.



1. Repository metadata layout
Every initialized MiniVCS repository contains a .minivcs/ directory.

.minivcs/
├── HEAD
├── config
├── tracked.txt
├── branches/
│   └── main
├── objects/
│   └── <commit-id>/
│       ├── commit.txt
│       └── files/
│           └── <committed-files>
├── retention.cfg
└── restore-staging/

Path	Contents	Written by
.minivcs/config	Repository format information	Repository::init()
.minivcs/HEAD	Current branch name	Repository and branch code
.minivcs/tracked.txt	One tracked relative path per line	FileManager
.minivcs/branches/<name>	Latest commit ID of that branch	CommitManager and branch code
.minivcs/objects/<id>/commit.txt	Commit metadata	CommitManager
.minivcs/objects/<id>/files/	Snapshot of committed files	CommitManager
.minivcs/objects/<id>/pruned	Marker that snapshot files were removed	RetentionManager
.minivcs/retention.cfg	Retention configuration	RetentionManager
.minivcs/restore-staging/	Temporary restore files and backups	CommitManager


2. Repository root discovery
Commands other than init search upward from the current working directory.

For example:

project/
├── .minivcs/
├── main.cpp
└── src/
    └── module/

Running a command from project/src/module/ must discover project/ as the repository root.

The repository root is the nearest ancestor containing a valid .minivcs directory. The search stops at the filesystem root.

The init command is different. It initializes the explicitly supplied path or the current directory.



3. Repository configuration
The current repository configuration is:

repository_format=1

This value is stored in:

.minivcs/config

A repository is considered initialized only when the required metadata directories and files exist and the configuration contains the expected repository format.



4. HEAD file
The file:

.minivcs/HEAD

contains only the current branch name.

Example:

main

Rules:

•	HEAD stores a branch name, not a commit ID.
•	The branch name must not be empty.
•	The branch name must not contain / or \.
•	The branch name must not be . or ...
•	The current branch pointer is stored separately in .minivcs/branches/<branch-name>.



5. Branch pointer files
Each branch has one pointer file:

.minivcs/branches/<branch-name>

The file contains one commit ID followed by a newline. A newly initialized branch may contain an empty line, meaning that the branch has no commits yet.

Rules:

•	A non-empty branch pointer must refer to an existing valid commit object.
•	A branch pointer must not contain unexpected additional data.
•	Branch pointers must be updated through a temporary file and replacement operation.
•	No module may create a second branch-pointer format.
•	Branch switching changes HEAD; it does not create a second commit store.



6. Tracked files
The file:

.minivcs/tracked.txt

contains one normalized relative path per line.

Rules:

•	Paths must be relative to the repository root.
•	Absolute paths are rejected.
•	Paths containing .. are rejected.
•	Paths inside .minivcs are rejected.
•	Paths use / as the separator.
•	Only regular files may be added.
•	Duplicate normalized paths are not allowed.
•	Ignored paths must be rejected by add.
•	add and status must use the same ignore rules.



7. Ignore rules
MiniVCS has these built-in ignored paths:

.minivcs
.git
.minivcsignore
.vs
.idea
.vscode
out
build
minivcs
minivcs.exe

Directories beginning with these prefixes are also ignored:

cmake-build-
build-

User-defined rules are read from:

.minivcsignore

Rules:

•	Blank lines are ignored.
•	Lines beginning with # are comments.
•	Trailing / is removed from directory rules.
•	A rule containing / matches the corresponding path.
•	A rule without / matches a path component.
•	Ignored files are not reported as untracked by status.
•	Ignored files cannot be added by add.

The shared implementation is:

include/IgnoreMatcher.h
src/vcs/IgnoreMatcher.cpp



8. Commit object layout
Each commit is stored in:

.minivcs/objects/<commit-id>/

The object contains:

.minivcs/objects/<commit-id>/
├── commit.txt
└── files/

The files/ directory contains the snapshot of every tracked file included in the commit.



9. Current commit format: version 1
Existing repositories currently use commit format version 1.

Each value is written using C++ std::quoted, in this order:

"<commit id>"
"<message>"
"<YYYY-MM-DD HH:MM:SS>"
"<parent commit id, empty for the first commit>"
<number of files>
"<relative path>"
"<sha-256 of the file>"
... one path/hash pair per file

Format version 1 supports:

•	Commit ID.
•	Commit message.
•	Timestamp.
•	One parent commit.
•	Number of files.
•	File paths.
•	SHA-256 file hashes.

Format version 1 does not yet support:

•	Author ID.
•	Second parent.
•	Merge commits.

The commit ID is calculated from the parent ID, commit message, timestamp value used for ID generation, and sorted file/hash entries.



10. Commit validation
A commit is valid only when:

•	commit.txt exists.
•	The stored commit ID matches the directory commit ID.
•	The metadata can be parsed completely.
•	The file count is valid.
•	Every path/hash pair is present.
•	File paths are relative and safe.
•	File hashes have the expected SHA-256 format.
•	No duplicate file paths exist.
•	No unexpected trailing metadata exists.
•	The parent commit exists when a parent ID is present.
•	The parent chain does not contain a cycle.
•	Snapshot files exist when the snapshot has not been pruned.
•	Snapshot file hashes match the hashes stored in commit.txt.

A damaged commit or branch pointer must not be reported as empty history.

Correct behavior:

Error: corrupt repository history: missing commit metadata for <commit-id>

Incorrect behavior:

No commits yet.

No commits yet. is valid only when the current branch exists and has an empty branch pointer.



11. Shared CommitManager operations
bool createCommit(const std::string& message);

Creates a commit from the current tracked working files. The current branch pointer is updated only after the commit object and snapshot have been written successfully.

HistoryResult readCommitHistory() const;

Reads and validates the current branch history.

struct HistoryResult {
    std::vector<Commit> commits;
    bool valid = true;
    std::string error;
};

Callers that need to distinguish an empty history from corruption should use readCommitHistory().

bool updateBranchPointer(
    const std::string& branchName,
    const std::string& commitId
) const;

Updates an existing branch pointer after validating the branch name and commit ID.

bool checkoutCommit(
    const std::string& requestedId,
    bool force = false
);

Checks out a commit snapshot into the working tree.

Checkout behavior:

•	Commit IDs and valid prefixes are accepted.
•	Ambiguous prefixes are rejected.
•	Missing commits are rejected.
•	Corrupt commit metadata is rejected.
•	Pruned snapshots are rejected.
•	Dirty working files are protected unless force is true.
•	The branch pointer is not changed by checkout.
•	HEAD is not changed by checkout.

bool restoreCommit(
    const std::string& requestedId,
    bool force = false
);

Restores a commit snapshot using the same safety rules as checkout. Restore does not move the current branch pointer.



12. Branch module rules
Member 2 owns branch and merge operations.

Member 2 should create separate files such as:

include/BranchManager.h
src/branch/BranchManager.cpp
include/MergeManager.h
src/branch/MergeManager.cpp

The branch module must:

•	Use .minivcs/HEAD.
•	Use .minivcs/branches/<branch-name>.
•	Use CommitManager::updateBranchPointer().
•	Use CommitManager::checkoutCommit().
•	Use CommitManager::getCommit().
•	Use the existing commit object storage.
•	Not create a second commit directory.
•	Not create a second branch-pointer format.
•	Not duplicate restore or snapshot validation logic.



13. Proposed commit format: version 2
Before branch/merge and authentication integration, the team must agree on the version-2 commit format.

The proposed format is:

"<commit id>"
"<message>"
"<YYYY-MM-DD HH:MM:SS>"
"<author id>"
"<first parent commit id, empty for the first commit>"
"<second parent commit id, empty for ordinary commits>"
<number of files>
"<relative path>"
"<sha-256 of the file>"
... one path/hash pair per file

Format version 2 rules:

•	author id is a stable user identifier.
•	The author ID is supplied by the authentication/database layer.
•	The first parent is the previous commit of the current branch.
•	The second parent is empty for an ordinary commit.
•	The second parent contains the merged branch tip for a merge commit.
•	Both parent IDs must refer to valid commit objects when present.
•	Author ID and both parent IDs are included in commit-ID calculation.
•	The reader must not silently interpret a version-1 record as version 2.
•	The team must decide whether to support both formats or migrate old repositories before version-2 writing is enabled.

No member may independently create a different commit format.



14. Retention rules
Retention operates on commit snapshot files.

Retention may remove:

.minivcs/objects/<commit-id>/files/

Retention must not remove:

.minivcs/objects/<commit-id>/commit.txt

Retention may create:

.minivcs/objects/<commit-id>/pruned

Before cleanup:

RetentionManager::setProtected(
    CommitManager::getBranchTips()
);

All branch tips must be protected from snapshot deletion.

After a snapshot is pruned:

•	History remains available.
•	commit.txt remains available.
•	Restore must refuse the pruned snapshot.
•	Diff must report that the snapshot was removed by retention.
•	Branch or merge operations must not assume pruned files exist.



15. Database module rules
Member 3 owns SQLite, authentication, and permissions.

SQLite may store:

Users
Repositories
Repository owners
Collaborators
Permissions
Retention settings
Optional commit indexes

The filesystem remains authoritative for:

Commit objects
Commit snapshots
Branch pointers
HEAD
Tracked files

SQLite must not silently become a second source of truth for branch pointers or commit metadata.

If SQLite stores commit IDs, those records are indexes and must be rebuildable from the filesystem.

Member 3 should use prepared statements and must never store plaintext passwords.



16. Remote module rules
Remote operations must use the same commit and snapshot formats as local repositories.

A remote repository may contain:

objects/
└── <commit-id>/
    ├── commit.txt
    └── files/

branches/
└── <branch-name>

Push and pull operations must:

•	Transfer commit.txt.
•	Transfer required snapshot files.
•	Validate received file hashes.
•	Transfer branch pointers only after required objects are complete.
•	Reject unauthorized operations.
•	Reject non-fast-forward updates unless explicitly supported.
•	Never silently overwrite local work.
•	Never invent a second commit format.



17. Shared ownership
Area	Owner
Repository discovery	Member 1
Core commit reading/writing	Member 1
File tracking and ignore rules	Member 1
Restore and checkout safety	Member 1
Branch creation and switching	Member 2
Merge and conflict detection	Member 2
SQLite metadata	Member 3
Authentication	Member 3
Permissions	Member 3
Remote repository	Member 4
Frontend	Member 4
Final integration	Member 1
Shared files must not be rewritten independently by multiple members:

CMakeLists.txt
src/main.cpp
include/Commit.h
src/vcs/commit.cpp
include/CommitManager.h
src/vcs/CommitManager.cpp
docs/INTERFACES.md

Changes to these files must be discussed before integration.



18. Integration requirements
Before a feature branch is merged:

cmake -S . -B build
cmake --build build --config Debug
python tests/run_tests.py <path-to-minivcs.exe>
ctest --test-dir build -C Debug --output-on-failure
git diff --check

The feature branch must document:

•	Files changed.
•	New public interfaces.
•	Tests added.
•	Test results.
•	Any changes to this interface contract.
•	Any known limitations.

Member 1 integrates feature branches one at a time through the integration branch. No member should push unfinished feature work directly to main.
