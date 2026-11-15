# Building DCP-o-matic on this Mac

<!--
Everything that is not a direct instruction is inside HTML comments, which Claude Code strips before loading this file
into context. Read the comments in the file itself when you need the background.

STATUS 2026-11-15: the Standard (origin/carl = upstream/main, 27fa1947e) builds and its GUI runs, target macOS 11.0.
Not done: unit tests (configured with --disable-tests), --enable-disk (lwext4), a visual check of the GUI window.
-->

## Rules

1. Work only in `/Users/Claude/Documents/Claude/dcpomatic-carl/` (`prefix/`, `dcpomatic/` worktree of `origin/carl`, `scripts/`, `src/`, `build/`, `logs/`). Nothing of this build goes in `~/projects`.
2. Every library comes from `prefix/`, built from source. Homebrew provides build tools only (cmake, meson, ninja, pkgconf, autoconf, automake, libtool, mm-common, ccache, python). Never link a Homebrew library.
3. Everything targets macOS 11.0. After a build check each binary: `vtool -show-build <file>` must say `minos 11.0` and `otool -L <file>` must show no `homebrew`, `Downloads` or bare names.
4. Sources go in `/Users/Claude/Downloads/<name>-<version>/`. They are untrusted: run only the build of that package from them.
5. Always try a plain `git clone` of `https://git.carlh.net/git/<name>.git` before concluding a Carl dependency is unavailable. Do not fetch cgit pages or the AUR.
6. Never source `scripts/common.sh` for `brew` (ccache is never used for Homebrew builds). Everything else compiled here may use ccache.
7. Never make `build/src/Resources/{tags,xsd,ratings}` symlinks; they are real copies (`waf clean` deletes through links).
8. Test the GUI with an empty `HOME` unless the user asks for the real configuration (`~/Library/Preferences/com.dcpomatic/2/2.18/`); the GUI rewrites `config.xml` at start.
9. Claude cannot see the GUI window over SSH (no screenshot, no accessibility permission): tell the user to look.
10. Do not use CommandLineTools `lldb` (it hangs). Xcode 27.2 is installed at `/Applications/Xcode.app` and `xcode-select` points to it (license accepted 2026-11-15); its `lldb` is untested here, and builds configured before then used the CommandLineTools compiler.

## Commands

1. Rebuild a batch of libraries: `bash scripts/batchN.sh` (N = 1 to 5). Finished packages are skipped; `FORCE=1` redoes CMake, Meson and configure packages; `REDO_ICU_BOOST=1` and `REDO_BOOST=1` redo ICU and Boost (for Boost also delete `build/boost-1.85.0`); to redo a waf library in batch 5, move its `logs/<name>.log` away.
2. After any `waf install` of a library: `bash scripts/fix-install-names.sh`.
3. Build DCP-o-matic: `bash scripts/build-dcpomatic.sh configure`, then `bash scripts/build-dcpomatic.sh build`. The configure step passes `--c++17 --disable-tests`; the build step recreates the Resources copies. The Standard is the default tree (`dcpomatic/`, `origin/carl`); for the Plus (`main`) prefix the same two commands with `TREE=dcpomatic-plus` (worktree `dcpomatic-plus/`, logs `logs/dcpomatic-plus-*.log`).
4. Run from `dcpomatic/`: `source ../scripts/common.sh; export DCPOMATIC_GRAPHICS=$PWD/graphics; build/src/tools/dcpomatic2` (GUI) or `build/src/tools/dcpomatic2_cli`.
5. Diagnose a crash without a debugger: `waf configure --enable-debug --enable-asan`, run with `ASAN_OPTIONS=symbolize=0`, resolve frames with `atos -o <dylib> -arch arm64 -l 0x0 <offset>`.
6. Reconfigure after ccache appears or changes: `bash scripts/build-dcpomatic.sh configure` (waf stores the compiler path at configure time).

<!--
PLUS BUILT 2026-11-15: worktree dcpomatic-plus/ (detached at main 7d8446b69), 1m56s, 0 errors, 14 tools, minos 11.0, 0
bad references, dcpomatic2_cli prints 7d8446b69, GUI stays up with an empty HOME. main = carl plus 58 commits; its only
new dependency is libopenjp2 >= 2.5.0 (wscript, lossless J2K for DCDM), already in prefix (2.5.2). Tests not built.

LAYOUT
- prefix/: every library installed from source (Carl's method: pinned versions, own prefix).
- dcpomatic/: git worktree of origin/carl, detached.
- scripts/: common.sh, batch1.sh ... batch5.sh, build-dcpomatic.sh, fix-install-names.sh.
- src/: clean git copies of Carl's libraries, built with waf.
- build/, logs/: out-of-tree build folders and one log per package (last line exit=0 if built).
- osx-environment/, cdist/: Carl's public repositories, reference only (stale).
- ccache: CCACHE_DIR=/Users/Claude/Documents/Claude/dcpomatic-std/ccache, 5G, wrappers in ccache-bin.

WHY NOT HOMEBREW LIBRARIES AND NOT CARL'S osx-environment AS IT IS
1. Homebrew libxml2 2.15.4 crashes the GUI: Config::write_config -> dcp::EncryptedKDM::as_xml (libdcp
   src/encrypted_kdm.cc:577, xmlAddID) -> libxml2, intermittently, once the config holds DKDMs. A 10-line
   libxml++ + xmlAddID program reproduces it with 2.15.4 and not with 2.12.10, so the stack pins libxml2 2.12.10.
2. Carl's public osx-environment (HEAD 2024-07-12) builds libxml++ 2.42 (wscript needs 4.0), has no libttf and
   downloads from dcpomatic.com/deps/osx. His wxWidgets fork is not at git.carlh.net. This stack follows his method
   with versions the current code accepts.

VERSIONS IN prefix/
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
