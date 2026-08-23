---
name: worktree
description: Use to create worktrees
---

# Create a worktree

Run this from the checkout whose `HEAD` should be the new branch's base:

```sh
scripts/create-worktree.sh <branch-name>
```

Do not call `git worktree add` directly. The script creates
`~/Dev/video-simili-<branch-name>` (replacing `/` with `-` in the directory
name) and runs `scripts/link-build-deps.sh` in the new worktree.
