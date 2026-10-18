# MiniVCS Shared Interfaces

This document defines the shared filesystem layout and interfaces used by all
MiniVCS modules.

Changes to the commit format, branch format, snapshot format, or repository
metadata must be discussed with all members before implementation.

---

## 1. Repository metadata layout

Every initialized MiniVCS repository contains a `.minivcs/` directory.

```text
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
