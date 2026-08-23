#!/usr/bin/env sh
# Link this worktree's gitignored build dependencies to the main checkout.
#
# The macOS Qt/OpenCV/FFmpeg/AOM builds are gitignored for their size, so a
# fresh worktree cannot configure or build until they are provided. They are
# several GB and identical for every checkout, so link them instead of copying.
#
# Safe to run repeatedly and from any directory inside the repository. Runs
# automatically from the git post-checkout hook and the Cursor sessionStart
# hook, but worktrees created by tools that skip hooks (Cursor's git worktree
# action among them) need `npm run link-build-deps`.
set -e

mainWorktree=$(dirname -- "$(git rev-parse --path-format=absolute --git-common-dir)")
worktree=$(git rev-parse --show-toplevel)

if [ "$mainWorktree" = "$worktree" ]; then
	exit 0
fi

cd "$worktree"

linkedCount=0

link_from_main() {
	relativePath=$1
	if [ -z "$relativePath" ]; then
		return 0
	fi
	if [ -e "$mainWorktree/$relativePath" ] && [ ! -e "$relativePath" ]; then
		mkdir -p "$(dirname "$relativePath")"
		ln -s "$mainWorktree/$relativePath" "$relativePath"
		linkedCount=$((linkedCount + 1))
	fi
}

# Wholly gitignored trees, one path per line relative to the checkout root.
# Paths cannot contain spaces: the loop below relies on word splitting.
ignoredTrees="
node_modules
QtProject/libraries/macos/qt/qt-install
"

for ignoredTree in $ignoredTrees; do
	link_from_main "$ignoredTree"
done

# The OpenCV/FFmpeg/AOM static archives are gitignored (`**/*.a`) while their
# headers are tracked, so those trees already exist here and can only be
# completed archive by archive.
macosLibraries="$mainWorktree/QtProject/libraries/macos"
if [ -d "$macosLibraries" ]; then
	# A subshell would lose linkedCount, so feed the loop without a pipe.
	while IFS= read -r archive; do
		link_from_main "${archive#"$mainWorktree/"}"
	done <<-EOF
		$(find "$macosLibraries" -name '*.a' \( -type f -o -type l \))
	EOF
fi

if [ "$linkedCount" -gt 0 ]; then
	echo "link-build-deps: linked $linkedCount build dependencies from $mainWorktree" >&2
fi
