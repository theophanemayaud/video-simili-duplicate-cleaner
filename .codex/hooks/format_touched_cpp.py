#!/usr/bin/env python3

import json
from pathlib import Path
import re
import shutil
import subprocess
import sys


CPP_EXTENSIONS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".m", ".mm"}
PATCH_FILE_PATTERN = re.compile(r"^\*\*\* (?:Add|Update) File: (.+)$", re.MULTILINE)
PATCH_MOVE_PATTERN = re.compile(r"^\*\*\* Move to: (.+)$", re.MULTILINE)


def patch_text(hook_input):
    tool_input = hook_input.get("tool_input", hook_input.get("toolInput", hook_input.get("input")))
    if isinstance(tool_input, str):
        return tool_input
    if isinstance(tool_input, dict):
        for key in ("patch", "input"):
            if isinstance(tool_input.get(key), str):
                return tool_input[key]
    return None


def main():
    try:
        hook_input = json.load(sys.stdin)
    except (json.JSONDecodeError, UnicodeDecodeError) as error:
        print(f"clang-format hook: invalid hook input: {error}", file=sys.stderr)
        return 1

    patch = patch_text(hook_input)
    if patch is None:
        print("clang-format hook: apply_patch input was not present in the hook payload", file=sys.stderr)
        return 1

    clang_format = shutil.which("clang-format")
    if clang_format is None:
        print("clang-format hook: clang-format is not on PATH; install it with `brew install clang-format`", file=sys.stderr)
        return 1

    repo_root = Path(subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True).strip()).resolve()
    raw_paths = PATCH_FILE_PATTERN.findall(patch) + PATCH_MOVE_PATTERN.findall(patch)
    files = []
    for raw_path in raw_paths:
        path = Path(raw_path.strip())
        if not path.is_absolute():
            path = repo_root / path
        path = path.resolve()
        try:
            path.relative_to(repo_root)
        except ValueError:
            print(f"clang-format hook: refusing to format path outside repository: {path}", file=sys.stderr)
            return 1
        if path.suffix.lower() in CPP_EXTENSIONS and path.is_file():
            files.append(path)

    if files:
        subprocess.run([clang_format, "-i", "--style=file", *map(str, dict.fromkeys(files))], check=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
