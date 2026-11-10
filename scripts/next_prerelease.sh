#!/bin/bash
# The next pre-release tag for a branch's head, for .github/workflows/prerelease.yml: GitHub Actions output lines
# (tag=..., skip=...). The version is the minor after the newest stable vX.Y.Z tag for main (main carries the features:
# 2.0.14 -> 2.1.0), or the release branch's own for a hotfix (release/vX.Y.Z); the number after -rc. is one more than that
# version's newest pre-release. skip=true when the head
# already has a pre-release tag (the nightly run with nothing new). Needs the tags fetched (fetch-depth: 0).
#
#   scripts/next_prerelease.sh <branch>
set -euo pipefail
branch=${1:-main}
head=$(git rev-parse HEAD)
if git tag --points-at "$head" | grep -Eq '^v[0-9]+\.[0-9]+\.[0-9]+-rc\.[0-9]+$'; then
    echo "skip=true"
    echo "tag=$(git tag --points-at "$head" | grep -E -- '-rc\.' | sort -V | tail -1)"
    exit 0
fi
if [[ $branch =~ ^release/v([0-9]+\.[0-9]+\.[0-9]+)$ ]]; then
    version=${BASH_REMATCH[1]}
else
    stable=$(git tag -l 'v[0-9]*' | grep -E '^v[0-9]+\.[0-9]+\.[0-9]+$' | sort -V | tail -1)
    [ -n "$stable" ] || { echo "no stable vX.Y.Z tag to count from" >&2; exit 1; }
    IFS=. read -r major minor patch <<<"${stable#v}"
    version="$major.$((minor + 1)).0"
fi
last=$(git tag -l "v$version-rc.*" | sed -E "s/^v$version-rc\.//" | { grep -E '^[0-9]+$' || true; } | sort -n | tail -1)
echo "skip=false"
echo "tag=v$version-rc.$(( ${last:-0} + 1 ))"
