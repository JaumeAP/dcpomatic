# DCP-o-matic public tree: branches, builds, rules and runbook

Status as of 2026-11-05. Public repository only: `~/projects/dcpomatic`, origin `JaumeAP/dcpomatic`,
upstream `git.carlh.net/git/dcpomatic.git` (Carl). Do not mention or touch the private repository here.
This file is committed on branch `claude-runbook` (commit `e0f1618fe`); later edits are not committed unless asked.

## 0. Rules (apply always)

1. Four lines, and only these four: Standard (`carl`), Plus (`main`), Beta (`beta`), Beta plus (`beta-plus`).
2. `carl` equals `upstream/main` and `beta` equals `upstream/v2.19.x`. They are only updated by fast-forward from
   upstream, never edited. `carl` and `beta` have their own rulesets (§1).
3. `main` (Plus) is updated whenever we say "plus": `carl` plus our open PRs, with its README list kept right.
4. `beta-plus` is `beta` plus our open PRs, taken from their Beta versions (`beta-compat/*`).
5. The undeletable ruleset `26609507` protects `main`, `carl`, `beta` and `beta-plus` against deletion. It does not
   stop force pushes on `main` or `beta-plus`.
6. Nothing is pushed to a PR head, to `main` or to `beta-plus` without explicit approval. Descriptions of PRs are not
   edited unless asked.
7. Every test of a PR to Carl runs on that PR's own Standard branch: `carl` plus only that PR's commits. A result for a
   PR is never taken from a combined branch (`plus-check`, `beta-plus`, `dcdm-series-7-on-carl`).
8. Commits: English; subject in the imperative or a noun phrase; end with the `Claude-Session` line only (no model names).
   Commits of unbuilt work start with `WIP:`.
9. `gh` works only with the sandbox off (it reads `~/.config/gh`); rulesets are created with `gh api -X POST
   repos/JaumeAP/dcpomatic/rulesets --input <json>`.
10. Build output goes to a log file in the build directory's parent; show only the last lines.

## 1. The four lines

1. **Standard** = Carl's `upstream/main`. Fork branch `carl`.
   - `carl` = `f6daea8d1` = `upstream/main` (checked 2026-11-05).
   - Ruleset `26591009` ("carl: fast-forward only, no delete"): no deletion, no non-fast-forward, no bypass. It cannot
     check that commits come from `upstream/main`.
   - Local worktree `~/projects/dcpomatic-std-build`, detached at `f6daea8d1`, with `test/data` at `3ca0f15` (the gitlink).
     Its DCDM series and the test correction are kept on the local branch `dcdm-series-7-on-carl` and the stash
     `dcdm-test-fix` (the stash can be dropped).
2. **Plus** = Standard + our open PRs. Fork branch `main`. Updated whenever we say "plus".
   - `origin/main` = `0b40358f5` (checked 2026-11-05): `carl` (`f6daea8d1`) plus 44 commits: the PR commits, the test
     correction for `dcdm_atmos_trimmed_test` (`b23e726ac`) and the README (`0b40358f5`).
   - The README (`README.md`) lists the pending PRs in plain words, 32 items, no #63. Keep it right when a PR is added,
     changed or taken by Carl.
   - Local worktree `~/projects/dcpomatic-plus-check`, branch `plus-check` (same commit as `origin/main`).
   - `origin/main` was force-pushed on 2026-11-05 from `737ad5618` (old Plus) with a lease. Its README was re-added in
     `0b40358f5` after the force push had dropped it.
   - The 256 unit-test cases of the 20 test files touched by the PRs pass on the Plus (2026-11-05, `b23e726ac`; the
     README commit changes no code).
3. **Beta** = Carl's `upstream/v2.19.x`. Fork branch `beta`.
   - `beta` = `14724280e` = `upstream/v2.19.x` (checked 2026-11-05), created on the fork that day.
   - Ruleset `26608646` ("beta: fast-forward only, no delete"): no deletion, no non-fast-forward, no bypass.
   - Local worktree `~/projects/dcpomatic-2.19`, detached at `14724280e`, `test/data` = `dbead97`
     (`/Volumes/INATECK 1TB/builds/test-data-2.19`) when tests run.
4. **Beta plus** = Beta + our open PRs, in their Beta versions. Fork branch `beta-plus`: NOT on the fork yet.
   - Local branch `beta-plus` in worktree `~/projects/dcpomatic-beta-plus`, from `14724280e`, 43 commits (2026-11-05).
   - Built: `beta-plus: build OK` (`scratchpad/build_beta_plus.sh`, output `/Volumes/INATECK 1TB/builds/dcpomatic-beta-plus/build`).
   - Every PR commit of the 12 `beta-compat/*` branches has its subject in `beta-plus`. Some commits are in it under
     another hash (the same content), and the duplicates were skipped.
   - Not yet run: the tests of `beta-plus`. The DCDM test correction (`b23e726ac`, Plus) is not in `beta-plus`; the
     Beta's own `dcdm` test is used.
   - How it was built, in this order (commit origin in brackets): the `atmos-mxf` stack (25 commits, covers PRs
     #60, #65, #66, #67, #68 to #86); `dkdm` (#61, 3 commits); `empty-subtitle` (#62, 2); `seven-one` (#70, 1);
     `atmos-hints` (#71 and #73, 2); `layout-hint` (#72, 2, except `75d5471e6`, the same content as the `seven-one`
     commit); `dcdm` (#87 to #95, 9). Commits with `cherry-pick -x`.
   - Rule for the pick list: `git rev-list --no-merges origin/beta-compat/<branch> --not 14724280e`. Without `--not`
     the whole history of the base comes in (this happened once: a junk commit `a12412b33` was picked and then reset).
   - `beta-compat/atmos-mxf` and `beta-compat/atmos-reference` hold different hashes for the same PR #60 commits. Use
     the `atmos-mxf` stack; do not mix both.

## 2. Open PRs and their Beta versions

All PRs are to `cth103/dcpomatic`; the heads are fork branches (`JaumeAP/dcpomatic`). Open PR count: 33 (#63 closed by Carl;
fork PR #16 for the same branch closed by us on 2026-11-05).

| PR (cth103) | Carl head branch (fork) | Beta branch (fork `beta-compat/*`) | Beta tests passing |
|---|---|---|---|
| 60 | atmos-reference-carl | atmos-reference | 12 |
| 61 | dkdm-reload-from-disk | dkdm | 24 |
| 62 | empty-open-subtitle-carl | empty-subtitle | 5 |
| 65 | video-mxf-decoder-ignore | video-mxf-ignore | 2 |
| 66 | atmos-decoder-ignore | atmos-decoder-ignore | 6 |
| 67 | content-untrimmed-clone | untrimmed-clone | 9 |
| 68 | sound-mxf-1 | sound-mxf (stack to #85) | 83 |
| 70 | 7-1-mask-mapping | seven-one | 10 |
| 71 | atmos-reels-hint | atmos-hints | 23 |
| 72 | non-cinema-layout-hint | layout-hint | 12 |
| 73 | atmos-reels-hint-dcp | atmos-hints | 23 |
| 74–85 | sound-mxf-1b … sound-mxf-14-no-recopy | sound-mxf | 83 |
| 86 | sound-mxf-15-atmos | atmos-mxf (also has #60, #66, #86) | 103 |
| 87–95 | dcdm-1-source-metadata … dcdm-9-gui | dcdm | 80 |

- Beta test counts are per changed files (`beta_check.sh`), "no errors: yes".
- Not ported to the Beta: `5fa3bc536` (arm64 reference frame in `remake_with_subtitle_test`, PR #86). Test-only, Apple silicon.
- Fork PR #16 (`hi-vi-mapping-carl`) closed 2026-11-05. Carl's `f6daea8d1` adapted #63.
- Comments posted on each of the 33 PRs (2026-11-05) say: Standard = this PR; Beta = `beta-compat/<branch>`.
  The original PR descriptions were not changed.
- PR #93 (`dcdm-7-film-encoder`): the head was pushed on 2026-11-05 from `4ae71546e` to `44422b22d` (fast-forward), adding
  the test correction. Its branch does not build on `carl` alone: `dcdm_audio_layout.h` comes from `dcdm-5`. Its own
  Standard test is `carl` plus the commits of `dcdm-1` … `dcdm-7`, in that order (clean cherry-picks).

## 3. Branches in the fork (`JaumeAP/dcpomatic`)

- 50 remote branches (counted 2026-11-05): `main`, `carl`, `beta`, the PR heads (§2), and 13 `beta-compat/*`:
  atmos-decoder-ignore, atmos-hints, atmos-mxf, atmos-reference, dcdm, dkdm, empty-subtitle, hi-vi, layout-hint, seven-one,
  sound-mxf, untrimmed-clone, video-mxf-ignore.
- `beta-plus` is not on the fork yet (§1.4).
- Local branches: 51 (2026-11-05), including `plus-check`, `beta-plus`, `dcdm-*`, `claude-runbook`.

## 4. libdcp: three lines

1. **Standard libdcp** = upstream libdcp `277f5ff2` (`labelled_channels`, needed by Carl's `main` `4cd35a37c`).
   Built in `~/projects/deps-libdcp-277` (prefix), from worktree `~/projects/libdcp-277` (detached at `277f5ff2`), by
   `scratchpad/build_libdcp277.sh` (needs `--c++17 --disable-examples --disable-dumpimage`).
2. **Plus libdcp** = `JaumeAP/libdcp` branch `plus` = `277f5ff2` (created 2026-11-05 from `cth103/libdcp`, which is public).
   No libdcp PR is open yet; when one is, it goes on `plus` only. Not built as a separate prefix yet: the Plus build
   currently uses the `277f5ff2` prefix from item 1. Needs its own prefix before a libdcp PR lands.
3. **Beta libdcp** = 1.10.66, in `~/projects/cdist-work-std-219` (already built).
   `cdist-work-std-main` also holds 1.10.66 and cannot build Carl's `main`.

## 5. Builds (4 cores, arm64 macOS)

- Standard: needs libdcp `277f5ff2`, so `bash ~/projects/build_carl_like.sh std` does not build (it uses the 1.10.66 of
  `cdist-work-std-main`: `dcp_examiner.cc:199` fails). Use `scratchpad/build_std_libdcp277.sh` (session scratchpad, not in
  the repository): tree `~/projects/dcpomatic-std-build`, output `/Volumes/INATECK 1TB/builds/dcpomatic-std-build/build`.
- Plus: `bash scratchpad/build_plus_check.sh` — tree `~/projects/dcpomatic-plus-check`, output
  `/Volumes/INATECK 1TB/builds/dcpomatic-plus-check/build`, libdcp prefix first in PKG_CONFIG_PATH, CXXFLAGS, LINKFLAGS,
  DYLD_LIBRARY_PATH. Logs: `carl_like_configure.log`, `carl_like_build.log` in the build directory's parent.
- Beta plus: `bash scratchpad/build_beta_plus.sh` — tree `~/projects/dcpomatic-beta-plus`, output
  `/Volumes/INATECK 1TB/builds/dcpomatic-beta-plus/build`, Beta libdcp `cdist-work-std-219`.
- Beta: `zsh scratchpad/beta_check.sh <beta-compat/branch>` — builds `~/projects/dcpomatic-2.19`, runs the tests of the
  files that the branch changes, returns to detached `upstream/v2.19.x`.
- Tests of the Plus: `zsh scratchpad/test_plus_check.sh <cases>`. The 256 cases of the 20 test files touched by the PRs
  pass on the Plus. The case list is built from the files of `f6daea8d1..HEAD`.
- Scratchpad scripts are in `/private/tmp/claude-501/-Users-jap-projects-dcpomatic-pro/24cfa4e7-5afa-4484-8bee-a54a381a6290/scratchpad/`.

## 6. Tooling notes (this machine)

- zsh: `"${b}:refs"` not `$b:refs`; `$TMPDIR` for temp files; no `sed -i`; reading files with Read, not Bash.
- `git cherry` and patch-id comparisons are not reliable when the commits were picked on different bases. Use hashes
  from the `(cherry picked from commit …)` trailers, and `--no-merges` ranges with `--not <base>`.
- Cherry-pick loop: on a failure, if `git status --porcelain` is empty, the commit is empty: `git cherry-pick --skip`.
  If a conflict is resolved by rerere ("all conflicts fixed"), compare the staged blobs or the patch with the commit
  before `GIT_EDITOR=true git cherry-pick --continue`. Stop at a real conflict.
- rerere replays old resolutions: review every staged result (`git diff --cached`) before continuing.
- `git push --force-with-lease=<ref>:<sha>` is the only force push used; `main` was force-pushed once (§1.2).

## 7. Open decisions (not done)

1. Create `beta-plus` on the fork (push) and its ruleset: waiting for approval.
2. Run the tests of `beta-plus`: pending.
3. Plus libdcp separate prefix (§4.2).
4. Dual-version PR text (done as comments, see §2). Descriptions not edited.

## 8. Test-data collections (`test/data`)

1. Standard `test/data` = `3ca0f15` (`dcpomatic-test-data`, Carl's main), in `~/projects/dcpomatic-std-build/test/data`.
2. Beta `test/data` = `dbead97` (`/Volumes/INATECK 1TB/builds/test-data-2.19`), the commit the Beta pins.
3. Paths that differ, Beta pin (`dbead97`) to Standard pin (`3ca0f15`), 12 entries:
   - Only in the Beta: `2.20.config.xml`, `2.20.config.sqlite.xml`, `2.20.config.grok.xml`, `2.20.config.sqlite.grok.xml`,
     `2.20.config.windows.xml`, `2.20.config.windows.sqlite.xml`, `metadata_from_218_loads_in_220.xml`,
     `playlists/17963124-6212-4f8f-b20f-092fe566396c.xml`, `playlists/ea1423e5-dc22-473c-81c8-41e40499502c.xml`.
     No `test/*.cc` names them (checked 2026-11-05).
   - Changed: `metadata.xml.ref`, `reels_test2/cpl_b1f91cd7-830c-43ca-97fe-bac388d33061.xml`,
     `reels_test4/cpl_c97c077f-1c1b-41ab-9549-71b399af1fa7.xml`.
4. Tests that read the changed paths: `film_metadata_test` (`metadata.xml.ref`); `film_reels_test`, `reels_test2`,
   `reels_test3`, `vf_test1` (`reels_test2`); `reels_test4`, `reels_test5` (`reels_test4`).
5. Beta check, 2026-11-05: tree `14724280e`, Beta data `dbead97`, cases above: exit 0, no errors.
6. Standard check, 2026-11-05: clean tree `f6daea8d1` (no PR commits), libdcp `277f5ff2`, own data `3ca0f15` (the gitlink
   of `carl`), same cases: exit 0, no errors.
