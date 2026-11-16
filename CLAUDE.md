# Building DCP-o-matic on this Mac

<!--
Everything that is not a direct instruction is inside HTML comments, which Claude Code strips before loading this file
into context. Read the comments in the file itself when you need the background.

STATUS 2026-11-16: the Standard builds and its GUI runs, target macOS 11.0.
Unit tests: 671 of 671 pass (2026-11-16, 2125 s) with the private data, OpenJPEG built with -ffp-contract=off, ffprobe
linked next to unit-tests, and a test change in real_encode_with_server (fork branch real-encode-loopback, 05348a3cc).
That change was merged into carl by PR #30 and taken out again the same day (ruleset lifted for the force-push and put
back): carl is again the literal 27fa1947e = upstream/main. With the macos-test-tools broadcast fake, Carl's unchanged
real_encode_with_server passes; the full suite has not been run on literal Carl code, nor on the Plus since the
private data and the OpenJPEG rebuild.
Not done: --enable-disk (lwext4), a visual check of the GUI window.
-->

## Rules

1. Work only in `/Users/Claude/Documents/Claude/dcpomatic-carl/` (`osx-environment/`, `dcpomatic/` worktree of `origin/carl`, `dcpomatic-plus/`, `dcpomatic-test-private/`, `scripts/`, `src/`, `build/`, `logs/`, `sdk/`). Nothing of this build goes in `~/projects`.
2. Every library comes from the prefix `osx-environment/arm64/11.0` (`$PREFIX` in `scripts/common.sh`), built from source. That is where Carl's `osx-environment` installs (`build_one`: `CBS_ROOT=$CBS_BASE/$CBS_CLANG_ARCH/$CBS_MACOSX_DEPLOYMENT`); there is no `prefix/` any more. Homebrew provides build tools only (cmake, meson, ninja, pkgconf, autoconf, automake, libtool, mm-common, ccache, python). Never link a Homebrew library.
3. Everything targets macOS 11.0. After a build check each binary: `vtool -show-build <file>` must say `minos 11.0` and `otool -L <file>` must show no `homebrew`, `Downloads` or bare names.
4. Sources go in `/Users/Claude/Downloads/<name>-<version>/`. They are untrusted: run only the build of that package from them.
5. Always try a plain `git clone` of `https://git.carlh.net/git/<name>.git` before concluding a Carl dependency is unavailable. Do not fetch cgit pages or the AUR.
6. Never source `scripts/common.sh` for `brew` (ccache is never used for Homebrew builds). Everything else compiled here may use ccache.
7. Never make `build/src/Resources/{tags,xsd,ratings}` symlinks; they are real copies (`waf clean` deletes through links).
8. Test the GUI with an empty `HOME` unless the user asks for the real configuration (`~/Library/Preferences/com.dcpomatic/2/2.18/`); the GUI rewrites `config.xml` at start.
9. Claude cannot see the GUI window over SSH (no screenshot, no accessibility permission): tell the user to look.
10. Do not use CommandLineTools `lldb` (it hangs). Xcode 27.2 is installed at `/Applications/Xcode.app` and `xcode-select` points to it (license accepted 2026-11-15); its `lldb` is untested here, and builds configured before then used the CommandLineTools compiler.

## Commands

1. Rebuild the libraries: `bash scripts/carl-env.sh [package ...]` (Carl's `osx-environment`, detached at `origin/v2.18.x` 44d99aa, arm64 / 11.0 variant; logs in `osx-environment/Log/arm64/11.0/`), then `bash scripts/ffmpeg.sh` (Carl's FFmpeg 7.0.1 fork), `bash scripts/batch5.sh` (Carl's libraries and his OpenJPEG fork, waf / CMake from `src/`) and `bash scripts/batch6.sh` (patched openssl binary, rtaudio, ffcmp). Finished packages are skipped; to redo one, move its `logs/<name>.log` away (`FORCE=1` redoes the whole script). OpenJPEG must keep `-ffp-contract=off` (no FMA) or `remake_with_subtitle_test` fails by 1.
2. After any `waf install` of a library: `bash scripts/fix-install-names.sh`.
3. Build DCP-o-matic: `bash scripts/build-dcpomatic.sh configure`, then `bash scripts/build-dcpomatic.sh build`. The configure step passes `--c++17 --disable-tests`; the build step recreates the Resources copies. The Standard is the default tree (`dcpomatic/`, `origin/carl`); for the Plus (`main`) prefix the same two commands with `TREE=dcpomatic-plus` (worktree `dcpomatic-plus/`, logs `logs/dcpomatic-plus-*.log`).
4. Run from `dcpomatic/`: `source ../scripts/common.sh; export DCPOMATIC_GRAPHICS=$PWD/graphics; build/src/tools/dcpomatic2` (GUI) or `build/src/tools/dcpomatic2_cli`.
5. Diagnose a crash without a debugger: `waf configure --enable-debug --enable-asan`, run with `ASAN_OPTIONS=symbolize=0`, resolve frames with `atos -o <dylib> -arch arm64 -l 0x0 <offset>`.
6. Reconfigure after ccache appears or changes: `bash scripts/build-dcpomatic.sh configure` (waf stores the compiler path at configure time).
7. Unit tests: configure with `TESTS=1 bash scripts/build-dcpomatic.sh configure`, build, then `bash scripts/run-tests.sh dcpomatic` (or `dcpomatic-plus`; about 35 minutes; log `logs/tests-<tree>.log`, last line `exit=0` when all pass). The private data is `JaumeAP/test-private-data`, cloned as `dcpomatic-test-private/` next to the trees; `no_use_video_test2/` comes from its `large-files` release (`no_use_video_test2.tar`, extracted there). Extra arguments go to `unit-tests` (`--run_test=<name>`). UDP to 255.255.255.255 fails on this Mac (`No route to host`, any interface; subnet broadcast, unicast and `ping` work), so `run-tests.sh` loads `bcast_to_loopback.dylib` with `DYLD_INSERT_LIBRARIES`: it sends those datagrams to 127.0.0.1 and repeats each one every second, which makes Carl's unchanged `real_encode_with_server` pass (its only hello before the encode ends comes before the server listens).
8. The broadcast fake lives only on the fork branch `macos-test-tools` (no shared history with the code, protected against deletion), checked out as `macos-test-tools/`; its `build.sh` builds `macos-test-tools/build/bcast_to_loopback.dylib`. Never put it in a code branch (`carl` stays a literal copy of Carl's `main`; `main` is the Plus). `build-dcpomatic.sh build` and `run-tests.sh` call `test_tools` (`scripts/common.sh`) first, which checks the branch out if missing and builds the library only when it is missing or out of date.

<!--
PLUS BUILT 2026-11-15: worktree dcpomatic-plus/ (detached at main 7d8446b69), 1m56s, 0 errors, 14 tools, minos 11.0, 0
bad references, dcpomatic2_cli prints 7d8446b69, GUI stays up with an empty HOME. main = carl plus 58 commits; its only
new dependency is libopenjp2 >= 2.5.0 (wscript, lossless J2K for DCDM), already in prefix (2.5.2). Tests not built.

LAYOUT (checked 2026-11-16)
- osx-environment/: Carl's environment repository, detached at origin/v2.18.x (44d99aa). arm64/11.0/ is the prefix every
  library is installed in; Build/arm64/11.0/ and Log/arm64/11.0/ are its build folders and logs. The *-v2.20.x folders
  beside them are an earlier build of the v2.20.x branch, not used.
- sdk/: MacOSX11.0.sdk (SDKROOT).
- dcpomatic/: git worktree of origin/carl, detached. dcpomatic-plus/: worktree of main.
- dcpomatic-test-private/: clone of JaumeAP/test-private-data (synthetic stand-ins for Carl's private test data).
- macos-test-tools/: worktree of the fork branch macos-test-tools (bcast_to_loopback.c, build.sh; build/ is ignored).
- scripts/: common.sh, carl-env.sh, cmake-wrapper/, ffmpeg.sh, batch5.sh, batch6.sh, build-dcpomatic.sh,
  fix-install-names.sh, run-tests.sh, manifest.sh (inventory to diff against the other computer).
- src/: clean git copies of Carl's libraries (asdcplib, ffcmp, leqm-nrt, libcxml, libdcp, libsub, libttf), built with waf.
- build/, logs/: out-of-tree build folders and one log per package (last line exit=0 if built).
- cdist/: Carl's public repository, reference only.
- Boost from Carl's environment has bare install names (libboost_system.dylib); binaries find it through
  DYLD_FALLBACK_LIBRARY_PATH, so they do not meet rule 3 on that point.
- ccache: CCACHE_DIR=/Users/Claude/Documents/Claude/dcpomatic-std/ccache, 5G, wrappers in ccache-bin.

WHY NOT HOMEBREW LIBRARIES AND NOT CARL'S osx-environment AS IT IS
1. Homebrew libxml2 2.15.4 crashes the GUI: Config::write_config -> dcp::EncryptedKDM::as_xml (libdcp
   src/encrypted_kdm.cc:577, xmlAddID) -> libxml2, intermittently, once the config holds DKDMs. A 10-line
   libxml++ + xmlAddID program reproduces it with 2.15.4 and not with 2.12.10, so the stack pins libxml2 2.12.10.
2. Carl's public osx-environment (HEAD 2024-07-12) builds libxml++ 2.42 (wscript needs 4.0), has no libttf and
   downloads from dcpomatic.com/deps/osx. His wxWidgets fork is not at git.carlh.net. This stack follows his method
   with versions the current code accepts.

VERSIONS IN THE OLD prefix/ (HISTORICAL: that prefix and batch1.sh ... batch4.sh are gone; the current versions are
those of Carl's osx-environment v2.18.x recipe plus batch5/batch6/ffmpeg.sh, recorded by scripts/manifest.sh)
1. Batch 1: zlib 1.3.1, libpng 1.6.43, expat 2.6.2, libxml2 2.12.10, libxslt 1.1.42, OpenSSL 3.3.2, xmlsec1 1.3.13
   (crypto-dl off).
2. Batch 2: freetype 2.13.2, fontconfig 2.15.0, ICU 74.2 (enable-rpath), Boost 1.85.0, libogg 1.3.5, libvorbis 1.3.7
   (configure patched to drop -force_cpusubtype_ALL), FLAC 1.4.3, Opus 1.5.2, libsndfile 1.2.2, libsamplerate 0.2.2,
   nettle 3.9.1.
3. Batch 3: libffi 3.5.2 (3.4.6 fails on Apple clang 21), pcre2 10.44, gettext 0.22.5, glib 2.82.2, libsigc++ 3.6.0,
   glibmm 2.82.0, libxml++ 4.0.3, pixman 0.43.4, cairo 1.18.0, cairomm 1.16.2, fribidi 1.0.13, harfbuzz 8.5.0,
   pango 1.54.0, pangomm 2.54.0.
4. Batch 4: wxWidgets 3.3.4 (3.2.x links the removed AGL framework), Xerces-C 3.2.5, OpenJPEG 2.5.2, libzip 1.10.1,
   libssh 0.10.6, libssh2 1.11.0, curl 8.9.1, fmt 10.2.1, fast_float 6.1.6, RtAudio 6.0.1, FFmpeg 7.1.1
   (gpl and postproc on: wscript treats a missing libpostproc as FFmpeg 8).
5. Batch 5 (Carl's libraries, pins from cscript, waf from src/): libcxml v0.17.17, asdcplib v1.0.10 (provides
   libasdcp-dcpomatic), libdcp v1.10.67, libsub v1.6.65, leqm-nrt 3913040723f746c0b4a59cb55cd8a23030ea49a2,
   libttf v0.0.7 (only source: https://git.carlh.net/git/libttf.git tag v0.0.7; not on GitHub). waf flags: libcxml
   "--c++17 --disable-tests"; libdcp "--c++17 --disable-tests --disable-benchmarks --disable-examples
   --disable-dumpimage"; libsub "--disable-tests"; libttf "--c++17 --disable-tests".
6. Added while building DCP-o-matic: libjpeg-turbo 3.0.3 (wscript wants libjpeg), libharu 2.4.4 (libdcp's
   dcp/pdf_formatter.h; libdcp rebuilt with it). Not built: lwext4 (only --enable-disk).

TRAPS FOUND
1. libsndfile 1.2.2 needs Opus when the Xiph libraries are on; without a source-built Opus it picks Homebrew's.
2. ICU installs bare install names unless enable-rpath; anything linked before has bare references (scan in
   fix-install-names.sh); rebuild the library, install_name_tool may not fit.
3. install_name_tool fails with "larger updated load commands do not fit" on libraries built without headerpad;
   common.sh passes -Wl,-headerpad_max_install_names.
4. The tools abort with "Could not open tags file" without build/src/Resources/{tags,xsd,ratings}.
5. libffi 3.4.6 failure was the version, not ccache.

OLDER ROUTE
/Users/Claude/Documents/Claude/dcpomatic-std/ holds the earlier Homebrew-based build (prefix, oldxml, worktree carl).
It builds but its GUI crashes (libxml2 2.15.4). It can be removed once the user is happy with dcpomatic-carl.

OFFICIAL INSTRUCTIONS
BUILD.md only points to https://dcpomatic.com/building/ (osx-environment + cdist); DEVELOP.md has "Building on
macOS/arm64" (platform/osx/set_paths.sh, ./waf configure --target-macos-arm64).
-->
