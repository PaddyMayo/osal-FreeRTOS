#!/usr/bin/env bash
# Creates a branch (if needed), commits any pending changes, pushes, and
# opens a PR against main.
#
# Usage:
#   scripts/gh-push.sh <branch-name> ["commit message"]
set -euo pipefail

if [ $# -lt 1 ]; then
  echo "Usage: $0 <branch-name> [\"commit message\"]" >&2
  exit 1
fi

branch="$1"
message="${2:-}"

current=$(git rev-parse --abbrev-ref HEAD)

if [ "$current" = "main" ]; then
  git checkout -b "$branch"
elif [ "$current" != "$branch" ]; then
  echo "Currently on branch '$current' (neither main nor '$branch') - switch branches manually first." >&2
  exit 1
fi

if [ -n "$(git status --porcelain)" ]; then
  if [ -z "$message" ]; then
    echo "You have uncommitted changes - pass a commit message as the second argument." >&2
    exit 1
  fi
  git add -A
  git commit -m "$message"
fi

git push -u origin "$branch"
gh pr create --fill --base main
