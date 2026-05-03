#!/bin/bash
set -euo pipefail

cpp_files=()
while IFS= read -r file; do
  case "$file" in
    *.c|*.cc|*.cpp|*.cxx|*.h|*.hh|*.hpp|*.hxx|*.m|*.mm)
      [[ -f "$file" ]] && cpp_files+=("$file")
      ;;
  esac
done < <(
  {
    git diff --name-only --diff-filter=ACMR -- QtProject/app QtProject/tests
    git diff --cached --name-only --diff-filter=ACMR -- QtProject/app QtProject/tests
    git ls-files --others --exclude-standard -- QtProject/app QtProject/tests
  } | sort -u
)

if (( ${#cpp_files[@]} > 0 )); then
  xcrun clang-format -i -style=file "${cpp_files[@]}"
fi

echo "{}"
