# Branch module (Member 2)

Files: include/branch.h, include/branchmanager.h, src/vcs/branch.cpp, tests/test_branching.cpp

Done: createBranch, listBranches, switchBranch (moves HEAD only), deleteBranch,
getCurrentBranch, getBranchCommit, getCurrentCommit. Storage uses .minivcs/HEAD and
.minivcs/branches/<name>. Unit test passes on macOS.

## For Member 1 to integrate
- Add src/vcs/branch.cpp to CMakeLists.txt (see docs/member2_integration.patch).
- I also touched repository.cpp so init creates HEAD and branches/ (see the patch).
  Please check it against your core-fixes version.
- No main.cpp changes made. Commands to wire: branch, branches, switch, delete-branch.

## Still to do on my side
- switchBranch(name, force) must call CommitManager::checkoutCommit and refuse dirty files.
- Merge needs the version-2 commit format (second parent) before I can create merge commits.

## Test
g++ -std=c++17 -Iinclude tests/test_branching.cpp src/vcs/branch.cpp -o /tmp/test_branching && /tmp/test_branching
