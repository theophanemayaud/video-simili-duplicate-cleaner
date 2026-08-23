#!/usr/bin/env sh
# Create a sibling development worktree and provision its shared dependencies.
set -eu

if [ "$#" -ne 1 ] || [ -z "$1" ]; then
	echo "usage: $0 <branch-name>" >&2
	exit 2
fi

branch=$1
git check-ref-format --branch "$branch" >/dev/null

repoRoot=$(git rev-parse --show-toplevel)
worktreeName=$(printf '%s' "$branch" | tr '/' '-')
worktreePath="$HOME/Dev/video-simili-worktree-$worktreeName"

if [ -e "$worktreePath" ]; then
	echo "create-worktree: destination already exists: $worktreePath" >&2
	exit 1
fi

if git show-ref --verify --quiet "refs/heads/$branch"; then
	git -C "$repoRoot" worktree add "$worktreePath" "$branch"
else
	git -C "$repoRoot" worktree add -b "$branch" "$worktreePath" HEAD
fi

"$worktreePath/scripts/link-build-deps.sh"

echo "create-worktree: ready at $worktreePath"
