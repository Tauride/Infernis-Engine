---
name: soc-port-workflow
description: Branch, pull-request, code-review and merge process for the Shadow of Chornobyl port on this Infernis fork. Use before creating a branch, committing, opening a pull request or merging anything for the SoC port.
---

# SoC port workflow

All Shadow of Chornobyl port work in this repository goes through `soc-infernis/main` on the fork
`Tauride/Infernis-Engine`. `main` mirrors upstream Infernis and receives no port work.

Follow the stages in order. Do not skip a stage, and do not merge before the code review is done.

## 1. Branch

- Start from an up-to-date `soc-infernis/main`:
  `git switch soc-infernis/main` then `git pull --ff-only`.
- Create the working branch from it, named `soc-infernis/<short-topic>`.
- Never branch from `main`.

## 2. Change and verify

- Keep engine changes small and isolated, so upstream Infernis updates keep merging cleanly.
- Keep planning and design documents out of this repository; they belong in the separate design-documentation
  repository.
- Build the affected targets (x64, `RelWithDebInfo`) and check the change in the game or with the tool it touches.
  Say plainly what was verified and what was not.

## 3. Pull request

- Push the branch to `origin` and open the pull request against `soc-infernis/main`. Pass the repository
  explicitly, because `gh` may otherwise resolve to the upstream repository:
  `gh pr create --repo Tauride/Infernis-Engine --base soc-infernis/main`
- Describe what changed, why, and how it was verified.

## 4. Code review

- Review the pull request diff with the `code-review` skill before any merge.
- Give the review the full pull request URL (`https://github.com/Tauride/Infernis-Engine/pull/<n>`), or read the
  diff with `gh pr diff <n> --repo Tauride/Infernis-Engine`. A bare `gh pr diff <n>` in this checkout returns the
  upstream repository's pull request with that number. Check that the diff is the one you opened before reviewing.
- Fix the findings on the same branch and push, or report the ones deliberately left open and why.
- Review again after substantial fixes.

## 5. Merge

- Merge only after the review stage is complete and the user has approved the merge.
- Merge with `gh pr merge <n> --repo Tauride/Infernis-Engine`, then update the local `soc-infernis/main`.

Every `gh pr` command in this checkout takes `--repo Tauride/Infernis-Engine`.
