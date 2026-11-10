# Releasing

Three kinds of release, from fastest to most careful. Versions and tags follow [versioning.md](versioning.md); stable
releases are cut by release-please's release PR (#1059). Nothing here involves game data: the release files are the
Builder, its setup list and the AppImage, built by CI from source.

## Pre-releases: any time, by anyone with write access

What is on `main` should reach testers within hours, not wait for the next stable release.

1. **Actions > Pre-release > Run workflow**, branch `main` (the default). Tick *dry run* first to see the tag it picks.
2. It tags `main`'s head as the next `vX.Y.Z-rc.N` (the patch after the newest stable release; N counts up) and starts
   the Windows builder and Linux AppImage workflows on that tag (`.github/workflows/prerelease.yml`,
   `scripts/next_prerelease.sh`).
3. About 30 minutes later the pre-release page has `HookshotBuilder.exe`, `hookshot-setup.txt` and the AppImage. It is
   marked *pre-release* and is never *latest*: the in-game update offer and the Builder's own update ask for *latest*,
   so players are not moved onto it. Testers download it from its page.

A pre-release of a commit that already has one is skipped. The nightly run (06:00 UTC) does the same for `main` while
the repository variable `PRERELEASE_NIGHTLY` is `true`; with it unset, pre-releases are only made by hand.

Before running it: `main`'s checks are green (Game-free source regressions, Windows builder, Linux AppImage). A
pre-release is for finding problems, not a test bench: it is not made from a branch that does not build.

## Stable releases: when a pre-release has held up

1. A pre-release has been out long enough to hear back (a day is a good default) with no new crash or regression
   reports on Discord or GitHub.
2. **The owner tests it**: a full local build from the same commit, played with nothing else running. Nothing becomes
   stable without that.
3. Merge release-please's open release PR (`chore: release <version>`), after editing its changelog entry if needed.
   Merging it tags the release, builds and attaches every file with `SHA256SUMS`, and publishes it as *latest* only once
   all of them are there (#1059).
4. Check the published release: the setup list at `releases/latest/download/hookshot-setup.txt` names the new version,
   and its parts download.
5. Close the issues the release fixes, with the version in the comment.

## Hotfixes: a stable release that cannot wait for main

For a fix to a stable release when `main` is not ready to release (it carries features still being tested):

1. Branch `release/vX.Y.Z` (the next patch) from the stable tag, and bring the fixes in through PRs against it,
   cherry-picked from `main` where they already landed there.
2. **Pre-release** with branch `release/vX.Y.Z`: it tags `vX.Y.Z-rc.N` on that branch, as above.
3. The owner tests it, then the stable tag `vX.Y.Z` goes on the same commit as the tested pre-release. The tag's
   builds publish it.

## What makes a release slow, and what this changes

- Before: every release was uploaded by hand (Windows' release job was off), and nothing went out between stable
  releases, so a day's merged work waited. Now: pre-releases go out from the Actions tab with no hand uploads, and
  stable releases are a merged PR.
- What stays deliberate: the owner's test before anything becomes *latest*.
