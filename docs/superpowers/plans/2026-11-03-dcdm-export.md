# DCDM Export Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Export a film as a DCDM: one 12-bit XYZ DPX file per frame plus one WAV file per audio channel, GUI and CLI, only when every video source is high quality.

**Architecture:** New `DCDMFilmEncoder : FilmEncoder` pulling `XYZ12LE` frames and audio from a `Butler` (as `FFmpegFilmEncoder` does), a standalone DPX writer, a standalone WAV writer, and a pure quality-gate function over content metadata shared by GUI and CLI. Examiners gain the metadata the gate needs.

**Tech Stack:** C++17, waf, Boost.Test (`test/`), FFmpeg libs (`AVCodecID`, `AVPixelFormat`), wxWidgets (GUI), libcxml (metadata).

**Spec:** `docs/superpowers/specs/2026-11-03-dcdm-export-design.md`

## Global Constraints

- Output video: DPX, CIE XYZ, 12 bits per component. No TIFF.
- Output audio: for embedded audio, one WAV per channel, never a multichannel WAV. Standalone PCM and Atmos are untouched originals.
- 3D: both eyes, `left/` and `right/` subdirectories; 2D writes DPX files directly in the output directory; audio written once.
- Gate: every video content must pass; DCP content fails; audio-only and text content ignored.
- Gate image formats: TIFF, DPX, EXR at 10 bits per component or more. JPEG and 8-bit PNG fail.
- Gate FFmpeg: intraframe/lossless codec AND 10+ bits AND chroma 4:2:2 or better. Accept ProRes 422 HQ, 4444, 4444 XQ; DNxHR HQX and 444; FFV1; v210; rawvideo. Fail everything else.
- Films with no stored codec/pixel-format metadata do not qualify until re-examined.
- GUI: DCDM menu item disabled with a tooltip/message naming the first failing content. CLI: error naming content and reason, non-zero exit.
- Reels: one reel writes the flat layout; several reels write `reel_01/`, `reel_02/`, ... each with the full layout, frame numbering restarting at 0, WAVs covering exactly the reel period.
- Originals untouched: copy each `AtmosMXFContent` file byte-identical to `atmos/` and each audio-only `FFmpegContent` file (no video) byte-identical to `audio/`, in the output root, original names; no cutting, no per-reel placement, no position/trim/gain applied; write `sources.txt` noting each original's position, trim and reel; name clash is an error.
- Embedded audio (audio inside a video source): rendered to one 24-bit mono WAV per channel from the video-bearing contents only.
- Open subtitles and closed captions: separate XML files (film's Interop/SMPTE format) with fonts, in `subtitles/` and `captions/`; not burnt into DPX.
- Existing exports must not change behaviour.
- Never include model identifiers in commits or code.

## Review Focus

- Film with zero video content: gate fails with a clear reason, no crash.
- Mixed film (one ProRes 4444 + one H.264 clip): gate fails and names the H.264 clip.
- Output directory already contains files: export errors out instead of silently overwriting.
- Odd frame rate or film length not a whole number of frames: frame count and audio length stay consistent, audio WAV lengths equal across channels.
- Multi-reel film: DPX counts per reel sum to the film length, every reel's WAVs have equal length, and no frame is dropped or duplicated at reel boundaries.
- Film with a standalone WAV plus a ProRes MOV with embedded audio: the WAV is copied untouched to `audio/` and the rendered per-channel WAVs contain only the MOV's audio (no double mix).
- Two originals with the same file name: export fails before any file is written.
- Film with Atmos content that is trimmed or offset: export fails with a clear message instead of writing an out-of-sync MXF.
- Film with closed captions only, or open subtitles only: only the matching folder is created.
- Film with more audio channels than the WAV naming scheme expects (e.g. 16): all channels written, names unique and sortable.

---

### Task 1: DPX writer

**Files:**
- Create: `src/lib/dpx_writer.h`, `src/lib/dpx_writer.cc`
- Modify: `src/lib/wscript` (add `dpx_writer.cc` to the source list)
- Test: `test/dpx_writer_test.cc`, `test/wscript`

**Interfaces:**
- Produces: `void write_dpx(boost::filesystem::path const& path, std::shared_ptr<const Image> image);` Precondition: `image->pixel_format() == AV_PIX_FMT_XYZ12LE`, else throws `PixelFormatError`. Throws `FileError` on write failure.

- [ ] **Step 1: Write failing tests** in `test/dpx_writer_test.cc`:
  - `dpx_writer_header_test`: write a 4x2 `XYZ12LE` image; read the file; assert magic is `0x53445058` ("SDPX", big-endian file), image width/height fields are 4 and 2, bit depth field of image element 0 is 12, file size field equals actual file size.
  - `dpx_writer_roundtrip_test`: write a 4x2 image with known component values (including 0, 4095 and a mid value); parse the pixel data back with a small test-local reader; assert every component equals the input.
  - `dpx_writer_rejects_wrong_format_test`: an `AV_PIX_FMT_RGB24` image throws `PixelFormatError`.
- [ ] **Step 2: Run** `./waf build && ./run/tests --run_test=dpx_writer_header_test,dpx_writer_roundtrip_test,dpx_writer_rejects_wrong_format_test`. Expected: build or tests FAIL (`write_dpx` undefined).
- [ ] **Step 3: Implement `write_dpx`** per SMPTE 268M: big-endian headers, 3-component image element, 12-bit components stored in 16-bit words. The builder fixes the remaining header values (descriptor, transfer, colorimetric, packing) and documents them in a header comment; DPX has no XYZ colorimetric code, so XYZ is by convention. Validate the output with an independent reader (ffprobe or ImageMagick) and note the result in the commit message.
- [ ] **Step 4: Run** the same command. Expected: PASS.
- [ ] **Step 5: Commit** `git add src/lib/dpx_writer.* src/lib/wscript test/dpx_writer_test.cc test/wscript && git commit -m "Add DPX writer for 12-bit XYZ images"`

### Task 2: WAV writer

**Files:**
- Create: `src/lib/wav_writer.h`, `src/lib/wav_writer.cc`
- Modify: `src/lib/wscript`
- Test: `test/wav_writer_test.cc`, `test/wscript`

**Interfaces:**
- Produces: `class WAVWriter { public: WAVWriter(boost::filesystem::path path, int sample_rate); void write(float const* samples, int frames); void close(); };` Mono, 24-bit PCM little-endian, header patched with final sizes on `close()`. Destructor calls `close()` if still open.

- [ ] **Step 1: Write failing tests**: `wav_writer_header_test` (write 100 frames at 48000; assert "RIFF"/"WAVE"/"fmt ", channels 1, sample rate 48000, 24 bits, data size 300); `wav_writer_samples_test` (write {0.0f, 1.0f, -1.0f, 0.5f}; assert decoded 24-bit values 0, 8388607, -8388608, 4194304 within ±1); `wav_writer_clips_test` (values beyond ±1.0 clamp, no wraparound).
- [ ] **Step 2: Run** `./waf build && ./run/tests --run_test=wav_writer_header_test,wav_writer_samples_test,wav_writer_clips_test`. Expected: FAIL.
- [ ] **Step 3: Implement `WAVWriter`** with a raw RIFF writer (no libsndfile in the repo). Use `dcp::File`/`Boost.Filesystem` consistent with neighbouring code.
- [ ] **Step 4: Run** same command. Expected: PASS.
- [ ] **Step 5: Commit** `git commit -m "Add mono 24-bit WAV writer"` (with the files above).

### Task 3: Persist source codec and pixel format

**Files:**
- Modify: `src/lib/ffmpeg_examiner.h/.cc` (add accessors), `src/lib/ffmpeg_content.h/.cc` (store, read and write XML, copy-from-ref, examine), `src/lib/image_examiner.h/.cc`, `src/lib/image_content.h/.cc`
- Test: `test/source_metadata_test.cc`, `test/wscript`

**Interfaces:**
- Produces on `FFmpegExaminer`: `AVCodecID video_codec_id() const; int video_profile() const; AVPixelFormat video_pixel_format() const;`
- Produces on `FFmpegContent`: `boost::optional<AVCodecID> video_codec_id() const; boost::optional<int> video_profile() const; boost::optional<AVPixelFormat> video_pixel_format() const;` XML tags `VideoCodecId`, `VideoProfile`, `VideoPixelFormat`.
- Produces on `ImageContent`: `boost::optional<AVPixelFormat> pixel_format() const;` XML tag `PixelFormat`, set from the decoded `image.image->pixel_format()` in `ImageExaminer`.
- All fields optional; absent in old metadata.

- [ ] **Step 1: Write failing tests**: `ffmpeg_content_codec_metadata_test` (add a ProRes file from `test/data`; after examine, `video_codec_id()` is `AV_CODEC_ID_PRORES`, pixel format is a 10-bit format; XML round-trip through `as_xml` and the node constructor preserves all three); `image_content_pixel_format_test` (add an existing test TIFF/PNG; `pixel_format()` is set and survives XML round trip); `old_metadata_test` (content XML without the new tags loads with all fields `boost::none`).
- [ ] **Step 2: Run** `./run/tests --run_test=ffmpeg_content_codec_metadata_test,image_content_pixel_format_test,old_metadata_test`. Expected: FAIL.
- [ ] **Step 3: Implement** accessors and persistence, mirroring how `_bits_per_pixel` is handled (`ffmpeg_content.cc` lines ~151, 213, 271, 307).
- [ ] **Step 4: Run** same command, then `./run/tests` for existing metadata tests. Expected: PASS.
- [ ] **Step 5: Commit** `git commit -m "Record source codec and pixel format in content metadata"`

### Task 4: Quality gate

**Files:**
- Create: `src/lib/dcdm_quality.h`, `src/lib/dcdm_quality.cc`
- Modify: `src/lib/wscript`
- Test: `test/dcdm_quality_test.cc`, `test/wscript`

**Interfaces:**
- Consumes: Task 3 accessors.
- Produces:
  - `struct DCDMQuality { bool ok; std::string reason; };` (`reason` empty when ok, otherwise names the content path and why.)
  - `DCDMQuality dcdm_source_quality(std::shared_ptr<const Film> film);`
  - `bool dcdm_ffmpeg_ok(AVCodecID codec, int profile, AVPixelFormat pix_fmt);`
  - `bool dcdm_image_ok(boost::filesystem::path first_file, AVPixelFormat pix_fmt);`

- [ ] **Step 1: Write failing tests** (pure functions first): `dcdm_ffmpeg_ok` true for ProRes profile 4444 on `AV_PIX_FMT_YUVA444P10LE`, ProRes HQ (422 HQ profile) on `YUV422P10LE`, FFV1 on `YUV422P10LE`, rawvideo on `YUV422P10LE`; false for ProRes Proxy, ProRes LT, ProRes 422 standard, H.264 on `YUV420P`, H.264 on `YUV422P10LE`, ProRes HQ on `YUV422P` (8-bit). `dcdm_image_ok` true for `.tiff` on `RGB48LE`, `.dpx` on `RGB48LE`, `.exr`; false for `.jpg`, `.png` on `RGB24`. Film-level: `dcdm_empty_film_test` (no video content: `ok == false`, non-empty reason), `dcdm_mixed_film_test` (good + bad clip: `ok == false`, reason contains the bad file's name), `dcdm_dcp_content_fails_test`, `dcdm_legacy_metadata_fails_test` (content with no stored codec fields fails).
- [ ] **Step 2: Run** `./run/tests --run_test=dcdm_*`. Expected: FAIL.
- [ ] **Step 3: Implement** the three functions. ProRes profiles use `FF_PROFILE_PRORES_*` constants; DNxHR via `FF_PROFILE_DNXHD_*`.
- [ ] **Step 4: Run** same command. Expected: PASS.
- [ ] **Step 5: Commit** `git commit -m "Add DCDM source quality gate"`

### Task 5: DCDMFilmEncoder

**Files:**
- Create: `src/lib/dcdm_film_encoder.h`, `src/lib/dcdm_film_encoder.cc`
- Modify: `src/lib/wscript`
- Test: `test/dcdm_film_encoder_test.cc`, `test/wscript`

**Interfaces:**
- Consumes: `write_dpx` (Task 1), `WAVWriter` (Task 2), `dcdm_source_quality` (Task 4).
- Produces: `DCDMFilmEncoder(std::shared_ptr<const Film> film, std::weak_ptr<Job> job, boost::filesystem::path output_directory);` implements `go()`, `current_rate()`, `frames_done()`, `finishing()` like `FFmpegFilmEncoder`. Constructor throws `std::runtime_error` carrying the gate reason if `!dcdm_source_quality(film).ok`. `go()` throws if `output_directory` exists and is not empty.
- Layout (one reel; several reels repeat it inside `reel_<2-digit n>/`): 2D `frame_<8-digit index>.dpx`; 3D `left/` and `right/` with the same names; audio `audio_<2-digit channel>.wav` in the output directory root.

- [ ] **Step 1: Write failing tests**: `dcdm_encoder_reels_test` (film with 2 reels from `film->reels()`: `reel_01/` and `reel_02/` each hold their frames numbered from 0 plus per-channel WAVs, counts sum to film length); `dcdm_encoder_2d_test` (short film from a qualifying test TIFF sequence; after the job: expected frame count of `.dpx` files, first file passes the Task 1 reader, one WAV per film audio channel, all WAVs same length); `dcdm_encoder_3d_test` (3D film: `left/` and `right/` each hold the frame count, one set of WAVs); `dcdm_encoder_rejects_low_quality_test` (H.264 content throws with the clip name in the message); `dcdm_encoder_nonempty_directory_test`.
- [ ] **Step 2: Run** `./run/tests --run_test=dcdm_encoder_*`. Expected: FAIL.
- [ ] **Step 3: Implement** the encoder: `Butler` with pixel format `AV_PIX_FMT_XYZ12LE`, `VideoRange::FULL`, audio enabled with a channel mapping of one input per output channel; loop structure and reel switching copied from `FFmpegFilmEncoder::go` (`reel_periods`, `file_encoders`) (`gets_per_frame` = 2 for 3D); do NOT burn open subtitles (text goes to Task 9 outputs).
- [ ] **Step 4: Run** same command. Expected: PASS.
- [ ] **Step 5: Commit** `git commit -m "Add DCDM film encoder"`

### Task 6: CLI

**Files:**
- Modify: `src/lib/encode_cli.cc` (options near lines 102, 291, 498-506, 609-613), `doc/` CLI man page if present
- Test: `test/encode_cli_test.cc`

**Interfaces:**
- Consumes: `DCDMFilmEncoder`, `dcdm_source_quality`.
- Behaviour: `--export-format dcdm --export-filename <directory>`; filename is the output directory.

- [ ] **Step 1: Write failing tests** in `test/encode_cli_test.cc`: `encode_cli_export_dcdm_test` (qualifying film: exit code 0, DPX and WAV files exist); `encode_cli_export_dcdm_low_quality_test` (H.264 film: error string contains the clip name and "quality", non-zero exit, no output directory content).
- [ ] **Step 2: Run** `./run/tests --run_test=encode_cli_export_dcdm_*`. Expected: FAIL (`--export-format` rejects `dcdm`).
- [ ] **Step 3: Implement**: accept `dcdm` in the format validation at line ~506, update the usage text (`out(...)` at line ~102 to list `mov`, `mp4` or `dcdm`), construct `DCDMFilmEncoder` in the export branch, check the gate before creating the job.
- [ ] **Step 4: Run** same command, then full `encode_cli_*`. Expected: PASS.
- [ ] **Step 5: Commit** `git commit -m "Add dcdm to CLI export formats"`

### Task 7: GUI

**Files:**
- Modify: `src/tools/dcpomatic.cc` (enum near line 245, binding near line 359, handler near line 1096, menu near line 1481, `set_menu_sensitivity`)
- Test: manual (GUI); no automated test

**Interfaces:**
- Consumes: `DCDMFilmEncoder`, `dcdm_source_quality`.
- Produces: menu item `Export DCDM...` (`ID_jobs_export_dcdm`, `NEEDS_FILM`), handler `DOMFrame::jobs_export_dcdm()`.

- [ ] **Step 1: Implement** the handler: run the gate; on failure `error_dialog(this, wxString::FromUTF8(reason))` and return; otherwise `wxDirDialog`, then create a `TranscodeJob` with `ChangedBehaviour::EXAMINE_THEN_STOP` and set the `DCDMFilmEncoder`, mirroring `jobs_export_subtitles`. Add `i18n` markers with `_()`.
- [ ] **Step 2: Gate-aware menu**: in `set_menu_sensitivity` enable the item only when `dcdm_source_quality(_film).ok`; set the menu help string to the reason when disabled.
- [ ] **Step 3: Build and check**: `./waf build`. Expected: build succeeds. Launch `run/dcpomatic` against a qualifying project and a low-quality project; confirm enabled/disabled state and a successful export on the qualifying one (use the `run` skill).
- [ ] **Step 4: Commit** `git commit -m "Add Export DCDM to the GUI"`

### Task 8: Original Atmos and PCM sources

**Files:**
- Modify: `src/lib/dcdm_film_encoder.h/.cc`
- Test: `test/dcdm_film_encoder_test.cc`

**Interfaces:**
- Consumes: `AtmosMXFContent::path(0)`, audio-only `FFmpegContent` (`video == nullptr`, `path(0)`), `Player(film, playlist, tolerant)`.
- Produces: `atmos/<original name>` and `audio/<original name>` byte-identical to the sources; `sources.txt` listing each original with position, trim and reel; the rendered-WAV path from Task 5 uses a playlist that excludes these contents.

- [ ] **Step 1: Write failing tests**: `dcdm_atmos_copy_test` (Atmos MXF from `test/data`: `atmos/<name>` byte-identical; multi-reel film: still one copy at root, untouched); `dcdm_pcm_original_copy_test` (standalone WAV: `audio/<name>` byte-identical); `dcdm_embedded_audio_render_test` (ProRes MOV with audio plus a standalone WAV: WAV copied, per-channel WAVs hold only the MOV's audio); `dcdm_original_name_clash_test` (two sources, same file name: throws before any file is written); `dcdm_sources_note_test` (`sources.txt` lists each original); `dcdm_no_originals_test` (no `atmos/`, `audio/` or `sources.txt`).
- [ ] **Step 2: Run** `./run/tests --run_test=dcdm_atmos_copy_test,dcdm_pcm_*,dcdm_embedded_*,dcdm_original_*,dcdm_sources_*,dcdm_no_originals_test`. Expected: FAIL.
- [ ] **Step 3: Implement** validation (name clashes) before anything is written, then the copies with `boost::filesystem::copy_file`, then the note. Build the Task 5 `Player` from a playlist of the video-bearing contents only.
- [ ] **Step 4: Run** same command. Expected: PASS.
- [ ] **Step 5: Commit** `git commit -m "Keep original Atmos and PCM sources untouched in DCDM export"`

### Task 9: Open subtitles and closed captions

**Files:**
- Modify: `src/lib/subtitle_film_encoder.h/.cc` (accept `TextType::CLOSED_CAPTION`, expose it per type), `src/lib/dcdm_film_encoder.h/.cc`
- Test: `test/dcdm_film_encoder_test.cc`, `test/export_subtitles_test.cc`

**Interfaces:**
- Consumes: `SubtitleFilmEncoder` output logic; film `interop()` selects XML flavour.
- Produces: `subtitles/` with the open subtitle XML and fonts, `captions/` with the closed caption XML; each only if the film has that kind. Existing subtitle export behaviour unchanged by default (closed captions only included when explicitly enabled by a new constructor argument `bool include_closed_captions = false`).

- [ ] **Step 1: Write failing tests**: `dcdm_open_subtitles_test` (film with `test/data/dcp_sub3.xml`: `subtitles/` holds a parsable subtitle XML, DPX frames unchanged by subtitles); `dcdm_closed_captions_test` (film with closed captions: `captions/` holds XML); `dcdm_text_reels_test` (2 reels: subtitles split per reel inside `reel_NN/`); `dcdm_no_text_test` (neither folder exists); in `export_subtitles_test.cc` a regression test that the default export still ignores closed captions.
- [ ] **Step 2: Run** `./run/tests --run_test=dcdm_*text*,dcdm_open_subtitles_test,dcdm_closed_captions_test`. Expected: FAIL.
- [ ] **Step 3: Implement**: extend `SubtitleFilmEncoder::text` filter at line ~126 behind the new constructor flag; run it inside the DCDM export into the two folders.
- [ ] **Step 4: Run** same command plus `export_srt_test`. Expected: PASS.
- [ ] **Step 5: Commit** `git commit -m "Export open subtitles and closed captions with DCDM"`

---

## Self-review notes

- Spec coverage: original Atmos and PCM (T8), text (T9), DPX (T1), per-channel WAV (T2), metadata prerequisite for the gate (T3), gate incl. DCP-fails and legacy-fails (T4), encoder incl. 3D layout (T5), CLI (T6), GUI with disabled state and reason (T7).
- Open risk for the user: DPX has no XYZ colorimetric code; XYZ is a convention, not a header flag. The design said "XYZ colorimetric set in the header"; T1 step 3 handles this by documenting the choice.
