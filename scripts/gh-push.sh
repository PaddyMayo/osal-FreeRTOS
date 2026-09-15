#!/usr/bin/env bash
# Pushes already-committed work and opens a PR against main.
#
# Run from a feature branch with commits ready: pushes it and opens the PR.
# Run from main with commits ready: creates a branch (named after HEAD's
# commit message) to hold them, pushes it, opens the PR, and deletes the
# local branch afterward (the remote copy lives on for the PR).
#
# Does not stage or commit anything - commit your changes first.
set -euo pipefail

current=$(git rev-parse --abbrev-ref HEAD)
created_branch=false

if [ "$current" = "main" ]; then
  if [ -z "$(git log origin/main..HEAD --oneline 2>/dev/null)" ]; then
    echo "No commits ahead of origin/main - commit your changes first." >&2
    exit 1
  fi

  slug=$(git log -1 --format=%s | tr '[:upper:]' '[:lower:]' | sed -E 's/[^a-z0-9]+/-/g; s/^-+|-+$//g')
  branch="${slug}-$(date +%s)"

  git checkout -b "$branch"
  created_branch=true
else
  branch="$current"
fi

git push -u origin "$branch"
gh pr create --fill --base main

if [ "$created_branch" = true ]; then
  git checkout main
  git branch -D "$branch"
fi
