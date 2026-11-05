# DCP-o-matic public tree: branches, builds and runbook

Status as of 2026-11-05. Public repository only: `~/projects/dcpomatic`, origin `JaumeAP/dcpomatic`,
upstream `git.carlh.net/git/dcpomatic.git` (Carl). This file is not committed; commit it only when asked.

## 1. The three lines

1. **Standard** = Carl's `upstream/main`. Fork branch `carl`, must equal `upstream/main`.
   - `carl` = `f6daea8d1` = `upstream/main` (checked 2026-11-05).
   - Ruleset `26591009` ("carl: fast-forward only, no delete"): no deletion, no non-fast-forward, no bypass.
     It cannot check that commits come from `upstream/main`.
2. **Plus** = Standard + our open PRs. Fork branch `main`. Updated whenever we say "plus".
   - `origin/main` = `0b40358f5` (checked 2026-11-05): `carl` (`f6daea8d1`) plus 44 commits: the PR commits, the test
     correction for `dcdm_atmos_trimmed_test` (`b23e726ac`) and the README (`0b40358f5`).
   - The README (`README.md`) lists the pending PRs in plain words, 32 items, no #63. Keep it right when a PR is added,
     changed or taken by Carl.
   - Local worktree `~/projects/dcpomatic-plus-check`, branch `plus-check` (same commit as `origin/main`).
   - `origin/main` was force-pushed on 2026-11-05 from `737ad5618` (old Plus) with a lease.
3. **Beta** = Carl's `upstream/v2.19.x`. Fork branch `beta`, must equal `upstream/v2.19.x`.
   - `beta` = `14724280e` = `upstream/v2.19.x` (checked 2026-11-05), created on the fork that day.
   - Ruleset `26608646` ("beta: fast-forward only, no delete"): no deletion, no non-fast-forward, no bypass.
   - Local worktree `~/projects/dcpomatic-2.19`, detached at `14724280e`.

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
- Local branches: 51 (2026-11-05), including `plus-check` and the `dcdm-*` branches.

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
  `cdist-work-std-main`: `dcp_examiner.cc:199` fails). Use the same script as the Plus (`build_plus_check.sh`) with
  `tree=~/projects/dcpomatic-std-build` and `out=/Volumes/INATECK 1TB/builds/dcpomatic-std-build/build`: that is
  `scratchpad/build_std_libdcp277.sh` in the session scratchpad (not in the repository).
- Plus: `bash scratchpad/build_plus_check.sh` — tree `~/projects/dcpomatic-plus-check`, output
  `/Volumes/INATECK 1TB/builds/dcpomatic-plus-check/build`, libdcp prefix first in PKG_CONFIG_PATH, CXXFLAGS, LINKFLAGS,
  DYLD_LIBRARY_PATH. Logs: `carl_like_configure.log`, `carl_like_build.log` in the build directory's parent.
- Beta: `zsh scratchpad/beta_check.sh <beta-compat/branch>` — builds `~/projects/dcpomatic-2.19`, runs the tests of the
  files that the branch changes, returns to detached `upstream/v2.19.x`.
- Tests of the Plus: `zsh scratchpad/test_plus_check.sh <cases>`. The 256 cases of the 20 test files touched by the PRs
  pass on the Plus (2026-11-05, before the README commit, which changes no code).

## 6. Tooling notes (this machine)

- zsh: `"${b}:refs"` not `$b:refs`; `$TMPDIR` for temp files; no `sed -i`; reading files with Read, not Bash.
- `git cherry` output with one commit is not a per-commit test; use `--no-merges` ranges and `git cherry-pick` with
  empty-skip (`git status --porcelain` empty) and stop at a real conflict.
- rerere replays old resolutions: review every staged result (`git diff --cached`) before `cherry-pick --continue`.
- `gh` works only with the sandbox off (it reads `~/.config/gh`). Rulesets are created with `gh api -X POST
  repos/JaumeAP/dcpomatic/rulesets --input <json>`; the JSON of the `carl` ruleset and the `beta` one are in the session
  scratchpad.

## 7. Open decisions (not done)

1. Plus libdcp separate prefix (item 4.2).
2. Dual-version PR text (done as comments, see §2). Descriptions not edited.
3. Push of the `dcdm-7-film-encoder` correction to the Carl PR: done (§2). Carl has not taken it.

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
