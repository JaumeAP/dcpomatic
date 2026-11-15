# DCP-o-matic public tree: branches, builds, rules and runbook

Status as of 2026-11-15 (the lines in §1 and the PR count in §2 were rechecked that day; the build, test and
Beta-version details were not, and still date from 2026-11-05). Public repository only: `~/projects/dcpomatic`, origin `JaumeAP/dcpomatic`,
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
   - `carl` = `27fa1947e` = `upstream/main` (checked 2026-11-15; 14 commits after `f6daea8d1`, among them
     `AudioMXFContent` and Carl's Atmos-reels hint).
   - Ruleset `26591009` ("carl: fast-forward only, no delete"): no deletion, no non-fast-forward, no bypass. It cannot
     check that commits come from `upstream/main`.
   - Local worktree `~/projects/dcpomatic-std-build`, detached at `ee8e4d9d3` (2026-11-15), with `test/data` at
     `3ca0f15` (the gitlink of `carl` and of `main`).
     Its DCDM series and the test correction are kept on the local branch `dcdm-series-7-on-carl` and the stash
     `dcdm-test-fix` (the stash can be dropped).
2. **Plus** = Standard + our open PRs. Fork branch `main`. Updated whenever we say "plus".
   - `origin/main` = `6033dadfa` (checked 2026-11-15): contains `carl` (`27fa1947e`, merged as `1b19ea31e`) and is 60
     commits ahead of `origin/carl`. Last change: PR #29 (`readme-drop-93`), merged with a merge commit.
   - The README (`README.md`) lists the pending PRs in plain words, 18 items (#61, #62, #70 as taken by Carl, #72,
     #97, #99, #75, #87 to #92, #94, #95, #105, #100, #101). Keep it right when a PR is added, changed, closed or
     taken by Carl.
   - PR #93 (`dcdm-7-film-encoder`) was closed without merge on 2026-11-14. Its code (`DCDMFilmEncoder`) is still in
     `main`, because #94 and #95 depend on it; it is no longer in the README list.
   - The worktree `~/projects/dcpomatic-plus-check` and the branch `plus-check` no longer exist. A Plus worktree is
     `~/projects/dcpomatic-std-plus`, branch `std-plus` (`509c4462e`, 2026-11-15).
   - `origin/main` was force-pushed on 2026-11-05 from `737ad5618` (old Plus) with a lease. Its README was re-added in
     `0b40358f5` after the force push had dropped it.
   - The 256 unit-test cases of the 20 test files touched by the PRs pass on the Plus (2026-11-05, `b23e726ac`; the
     README commit changes no code).
3. **Beta** = Carl's `upstream/v2.19.x`. Fork branch `beta`.
   - `beta` = `462fd34df` = `upstream/v2.19.x` (checked 2026-11-15; it was `14724280e` on 2026-11-05).
   - Ruleset `26608646` ("beta: fast-forward only, no delete"): no deletion, no non-fast-forward, no bypass.
   - Local worktree `~/projects/dcpomatic-2.19` (on 2026-11-15: branch `loose-reuse-always`, `452f76287`), `test/data` = `dbead97`
     (`/Volumes/INATECK 1TB/builds/test-data-2.19`) when tests run.
4. **Beta plus** = Beta + our open PRs, in their Beta versions. Fork branch `beta-plus`: ON the fork now
   (`origin/beta-plus` = `aec2e162c`, 2026-11-15).
   - Local branch `beta-plus` in worktree `~/projects/dcpomatic-beta-plus` (`1456be34e`, 2026-11-15). On 2026-11-05 it
     was from `14724280e` with 43 commits.
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

All PRs are to `cth103/dcpomatic`; the heads are fork branches (`JaumeAP/dcpomatic`). Open PR count: 18 on 2026-11-15
(#61, #62, #72, #75, #87 to #92, #94 to #97, #99, #100, #101, #105). #63 was closed by Carl; fork PR #16 for the same branch
was closed by us on 2026-11-05. The PRs of 2026-11-05 that are no longer open (#60, #65 to #71, #73, #74, #76 to #86) were
taken by Carl or closed by us; which is which was not checked one by one. #93 was closed without merge on 2026-11-14.

| PR (cth103) | Head branch (fork) | Base | Beta branch (fork `beta-compat/*`) | Beta tests passing (2026-11-05) |
|---|---|---|---|---|
| 61 | dkdm-reload-from-disk | main | dkdm | 24 |
| 62 | empty-open-subtitle-carl | main | empty-subtitle | 5 |
| 72 | non-cinema-layout-hint | main | layout-hint | 12 |
| 75 | sound-mxf-2 | main | sound-mxf | 83 (stack, before the 2026-11-09 update) |
| 87 | dcdm-1-source-metadata | main | dcdm | 80 (#87 to #95 together) |
| 88 | dcdm-2-wav-writer | main | dcdm | same |
| 89 | dcdm-3-dpx-writer | main | dcdm | same |
| 90 | dcdm-4-quality-gate | main | dcdm | same |
| 91 | dcdm-5-audio-layout | main | dcdm | same |
| 92 | dcdm-6-j2k-writer | main | dcdm | same |
| 94 | dcdm-8-cli | main | dcdm | same |
| 95 | dcdm-9-gui | main | dcdm | same |
| 96 | atmos-dcpasset | `v2.19.x` | none: the PR is already the Beta version | 64 cases (in the PR text) |
| 97 | listen-hi-vi | main | listen-hi-vi | not recorded |
| 99 | fix-referenced-ccap-as-subtitle | main | referenced-ccap | not recorded |
| 100 | fix-sign-language-channel-check | main | sign-language-channel-check | not recorded |
| 101 | sign-language-mca-label | main | sign-language-mca-label | not recorded |
| 105 | dcdm-10-import | main | none yet (`beta-compat/dcdm` has no `dcdm_import`) | not recorded |

- Heads of #61, #62, #72 and #87 to #95 are from the 2026-11-05 table; the others were read from the PRs on 2026-11-15.
  #101 needs libdcp#18 (cth103/libdcp). #94, #95 and #105 build on the DCDM series (#87 to #92 and the code of the
  closed #93, which is in `main`).
- Beta test counts are per changed files (`beta_check.sh`), "no errors: yes".
- Not ported to the Beta: `5fa3bc536` (arm64 reference frame in `remake_with_subtitle_test`, PR #86). Test-only, Apple silicon.
- Fork PR #16 (`hi-vi-mapping-carl`) closed 2026-11-05. Carl's `f6daea8d1` adapted #63.
- Comments posted on each of the 33 PRs (2026-11-05) say: Standard = this PR; Beta = `beta-compat/<branch>`.
  The original PR descriptions were not changed.
- PR #93 (`dcdm-7-film-encoder`, closed 2026-11-14, head `44422b22d`): its branch does not build on `carl` alone:
  `dcdm_audio_layout.h` comes from `dcdm-5`. Its own Standard test was `carl` plus the commits of `dcdm-1` … `dcdm-7`,
  in that order (clean cherry-picks). The branch stays on the fork.

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

1. Done: `beta-plus` is on the fork (`origin/beta-plus` = `aec2e162c`, 67 commits over `origin/beta`, 2026-11-15).
   Its ruleset is the shared `26609507` (deletion only); it has no fast-forward ruleset of its own (§0.5). Checked by commit subject, not by content: it holds #97, #99, #100, #101, #61, #62, #72, #75
   (whether its `sound-mxf` stack is the version from before or after 2026-11-09 was not checked) and #87 to #95.
   It lacks #96 (`atmos-dcpasset`, 2 commits; no `atmos_is_reusable`) and #105 (no Beta version of the DCDM import
   exists: no `dcdm_import` in `beta-plus` or in `beta-compat/dcdm`). Adding them needs approval (§0.6).
2. Run the tests of `beta-plus`: not known to be done.
3. Plus libdcp separate prefix (§4.2). #101 depends on libdcp#18, so this is now due.
4. Dual-version PR text: done as comments on the 33 PRs of 2026-11-05 (§2). The PRs opened since (#96 to #105) have
   no such comment. Descriptions not edited.
5. Decide what to do with the `DCDMFilmEncoder` code in `main` now that #93 is closed (the README no longer lists
   it): keep it, or remove it together with #94, #95 and #105.

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

## 9. Build stacks on the two machines (read 2026-11-15)

Neither machine follows Carl's recipe as it stands. The two stacks differ, so a result from one is not a result for
the other. The Mac mini inventory comes from a read-only `manifest.sh` run (full file in the session scratchpad, not
kept); the Mac Studio description comes from its own `CLAUDE.md`, read over `ssh macstudio`. Not verified: the real
library versions in the Mac Studio `prefix/` (it lives in iCloud, which the path guard blocks).

1. **Mac mini** (this machine). Recipe: `~/projects/osx-environment-upstream`, commit `44d99aa` (branches
   `fix/libssh-cmake4`, `v2.18.x`), with local edits to `config.sh`: `-j10`, SDK 26.5 from Xcode, `CBS_BUILD_ARCH=aarch64`,
   variant arm64. Dependency prefix `~/projects/cdist-work-std-main`, deployment target `minos=11.0`, no Homebrew or
   `/usr/local` path in any dependency line. Library versions: libdcp 1.10.67, libsub 1.6.65, libcxml 0.17.17,
   libasdcp-dcpomatic 1.0.10, libopenjp2 2.5.2, FFmpeg 7.0.1 (libavcodec 61.3.100). Archives of the recipe: Boost
   1.84.0, libxml2 2.12.3, OpenSSL 3.2.0, ICU 74.1, wxWidgets 3.2.12, dav1d 0.9.2. `libdcp` for Standard and Plus is
   built apart in `~/projects/deps-libdcp-277` (§4). Trees on 2026-11-15: `dcpomatic-std-build` at `ee8e4d9d3` (carl
   plus the #75 commit, 1 uncommitted file, not `origin/carl`), `dcpomatic-std-plus` at `509c4462e`,
   `dcpomatic-beta-plus` at `1456be34e`, `dcpomatic-2.19` at `452f76287`. All four binaries fail to start with
   `DYLD_FALLBACK_LIBRARY_PATH=<prefix>/lib`: `Library not loaded: libboost_system.dylib` (cause not looked into).
2. **Mac Studio** (`ssh macstudio`, account `Claude`). Public repo `/Users/Claude/projects/dcpomatic` on `main` at
   `7d8446b69` (before PR #29; its `origin/carl`, `origin/beta` and `origin/beta-plus` match ours), with an untracked
   `CLAUDE.md`. The build is in `~/Documents/Claude/dcpomatic-carl/` (not in `~/projects`), from source into its own
   `prefix/` with Carl's method, Homebrew only for build tools, target macOS 11.0. Built 2026-11-15: Standard
   (`origin/carl` `27fa1947e`) and Plus (`main` `7d8446b69`, 1 m 56 s, 14 tools), both with `--disable-tests`, so no
   unit tests, no `--enable-disk` (lwext4), GUI window not looked at. Versions in its `CLAUDE.md`: Boost 1.85.0,
   libxml2 2.12.10, OpenSSL 3.3.2, ICU 74.2, wxWidgets 3.3.4, FFmpeg 7.1.1, libdcp 1.10.67, libsub 1.6.65, libcxml
   0.17.17, asdcplib 1.0.10. It pins libxml2 2.12.10 because Homebrew's 2.15.4 crashes the GUI in
   `dcp::EncryptedKDM::as_xml` (`xmlAddID`). Its `osx-environment` and `cdist` clones are reference only. An older
   Homebrew-based build in `~/Documents/Claude/dcpomatic-std/` builds but its GUI crashes; it can go when the user
   is happy with `dcpomatic-carl`.
3. **Mismatch to settle.** The Mac Studio session first said its stack was `osx-environment` v2.20.x (`1ba55eb`) with
   Boost 1.89, libxml2 2.15.1, OpenSSL 3.6.0, ICU 78.1, wxWidgets 3.3.1; its own `CLAUDE.md` says otherwise (list in
   item 2). Which stack is the reference for Standard and Plus is not decided; the user was told.
