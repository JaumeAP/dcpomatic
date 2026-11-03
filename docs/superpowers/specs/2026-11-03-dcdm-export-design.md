# DCDM export: design

## Intent

Add an export that writes a Digital Cinema Distribution Master from a film: one 12-bit XYZ DPX file per frame, plus audio as one WAV file per channel. Users need an uncompressed, colour-space-correct master that other cinema tools can ingest.

## Goals

- Export a film as a DPX frame sequence, CIE XYZ, 12 bits per component.
- Original Atmos and PCM source files are kept untouched (user decision, see below). Audio that has no standalone original file (embedded in a video file) is exported as one WAV file per channel.
- Available in the GUI (export dialog) and the CLI (`encode_cli`).
- Fits the existing `FilmEncoder` pattern, so progress, cancel and job handling work as for the other exports.

## Non-goals

- TIFF output.
- Single multichannel WAV.
- Cutting, re-wrapping, remixing or applying gain to Atmos or standalone PCM sources. (Resampling is done only when a PCM source rate differs from the film's audio frame rate.)
- Editing or overwriting existing DCDMs.
- DCP creation changes.

## Anti-goals

- Lossy conversion through RGB: pixels must stay in XYZ end to end.
- A DPX writer that produces files other tools reject.

## Constraints

- No DPX or TIFF writing exists in the repo today.
- `PlayerVideo::image(pixel_format_fn, video_range, fast)` can already return `AV_PIX_FMT_XYZ12LE` (the DCP path in `dcp_video.cc` does this).
- FFmpeg's `dpx` encoder does not take `XYZ12LE` input and does not tag the colour space as XYZ, so it cannot be reused.
- Existing encoders to mirror: `FFmpegFilmEncoder`, `SubtitleFilmEncoder`, `DCPFilmEncoder`. GUI hook point: `src/tools/dcpomatic.cc` (SubtitleFilmEncoder is created near line 1127). CLI hook point: `src/lib/encode_cli.cc`.

## Approaches

A. New `DCDMFilmEncoder : FilmEncoder` with its own DPX writer from XYZ12 data, audio through a WAV writer. Chosen.
B. Reuse `FFmpegFileEncoder` with the `dpx` codec. Rejected: no XYZ12 input, no XYZ tagging, would force an RGB round trip.

The user chose A and asked for GUI plus CLI.

## Design (as decided)

- New class `DCDMFilmEncoder` in `src/lib/dcdm_film_encoder.{cc,h}`, added to `src/lib/wscript`.
- Video: for each frame, request `AV_PIX_FMT_XYZ12LE` from the `Butler`/`PlayerVideo`, write `frame_<index>.dpx` (zero-padded) into the output directory. 12-bit components packed into 16-bit words per SMPTE 268M, XYZ colorimetric set in the header.
- Audio: one WAV per channel, named by channel, at the film's audio rate.
- 3D: export both eyes. User decision. Layout is my call: `left/` and `right/` subdirectories, each with its own DPX sequence. Audio is written once, not per eye. 2D films write DPX files directly in the output directory.

## Quality gate (user decision, thresholds are my calls)

The export is only available when every video content in the film is high enough quality. One failing video content disables it. Audio-only and text content are ignored.

A video content passes if it is one of:

- Image sequence/still: TIFF, DPX or EXR, at 10 bits per component or more. JPEG and 8-bit PNG fail.
- FFmpeg file: intraframe or lossless codec AND bit depth of 10 or more AND chroma 4:2:2 or better. Accepted codecs: ProRes 422 HQ, 4444, 4444 XQ (not Proxy, LT, 422); DNxHR HQX and 444 (not LB, SQ, HQ 8-bit); FFV1; v210; rawvideo. Everything else fails, including DCP content (H.264, HEVC, MPEG-2, XDCAM, plain ProRes 422).
- DCP content: fails. User decision. A DCP is already the final deliverable, and turning a DCDM back into a DCP would re-encode JPEG 2000 (generation loss).

Rationale: DCDM is a master; exporting 8-bit or heavily compressed sources into 12-bit XYZ would pretend quality that does not exist.

Behaviour when the gate fails:

- GUI: DCDM entry disabled, tooltip names the first failing content and why.
- CLI: refuse with an error naming the content and reason, non-zero exit.

Consequences:

- Content metadata does not currently record codec, pixel format or bit depth. `FFmpegExaminer` and `ImageExaminer` must be extended to store them, and `FFmpegContent`/`ImageContent` must persist them in metadata.xml.
- Existing films without the new fields are treated as not qualifying until the content is re-examined. This is my call; it avoids false passes.
- The gate is a pure function of content properties, placed in lib so GUI and CLI share it.

## Atmos and text (user decision: include them)

- Original sources are left untouched (user decision, replaces the earlier per-reel Atmos cut and the all-audio WAV render).
  - Dolby Atmos: every `AtmosMXFContent` source file is copied byte-identical to `atmos/` in the output root, original file name.
  - Standalone PCM (user decision: PCMs are always delivered split per channel): every audio-only source file (an `FFmpegContent` with no video, e.g. a WAV) is split into one mono WAV per channel in `audio/`, named `<source stem>_ch<NN>.wav`. All WAVs are 24-bit (user decision: audio is converted to 24 bits whenever the source is not). 16-bit sources are widened exactly (sample value shifted left by 8, no loss); 32-bit integer and float sources are rounded to 24 bits, no dither (my call). Sample rate (user decision): always the film's audio frame rate, i.e. the rate the final DCP will have (`film->audio_frame_rate()`); sources at another rate are resampled. No remix or gain.
  - Audio embedded in a video source (e.g. a ProRes MOV): rendered the normal way into standard PCM: one 24-bit mono WAV per channel at the film's audio frame rate, from those contents only.
  - My calls: originals are not split per reel and are not placed in `reel_NN/`; position, trim and gain are not applied to them, so their sync information is lost (for PCM as well as Atmos) in the output; a short note about this is written next to them (see builder items). Name clashes between Atmos originals, or between PCM stems, are an error.
- Open subtitles and closed captions: written as separate files, not burnt into the DPX frames (my call; the user did not choose between this and burning). Format follows the film (Interop or SMPTE XML), with fonts. Files go in `subtitles/` (open) and `captions/` (closed). Nothing is written if the film has none.
- Consequence: the DCDM encoder must not call `set_always_burn_open_subtitles()`.
- `SubtitleFilmEncoder::text` currently drops everything but `TextType::OPEN_SUBTITLE`; closed captions need support there or in a shared helper.

## Audio layouts: 5.1 and 7.1 only (user decision)

- Exported audio is only ever 5.1 or 7.1. If the programme is 7.1 (rear surround channels in use), both a 7.1 set and a 5.1 set are written; if it is 5.1 or smaller, only the 5.1 set.
- Sets are directories of mono 24-bit WAVs: `audio_5.1/` and `audio_7.1/`, files named by channel label: 5.1 = L, R, C, LFE, Ls, Rs; 7.1 adds Lrs, Rrs (`dcp::Channel` BSL, BSR). Rendered audio sets live in each reel folder (output root for one reel). PCM source splits live in the output root, prefixed with the source stem: `<stem>_<label>.wav`.
- 5.1 from 7.1 is a downmix: Ls' = Ls + g*Lrs, Rs' = Rs + g*Rrs with g = -3 dB (my call), other channels unchanged. Rear channels are folded into the side surrounds, never dropped.
- My calls: a programme with fewer than six channels is padded with silent channels so the 5.1 set is always complete; a programme using more than 8 channels (or HI/VI/Lc/Rc) fails the export with a clear error naming the channels.
- Applies to rendered embedded audio and to PCM source splits alike.

## Reels (user decision: respect them)

- The export follows `film->reels()`. One reel: flat layout as above. More than one reel: one subdirectory per reel, `reel_01/`, `reel_02/`, ..., each holding the per-reel layout (DPX frames or `left/` and `right/`, rendered audio WAVs, subtitles, captions). Frame numbering restarts at 0 in each reel; WAVs cover exactly the reel's period.
- Atmos and standalone PCM originals are not per reel (see above). Rendered WAVs for embedded audio, DPX, subtitles and captions are per reel.
- Subtitles and captions are split per reel, like `SubtitleFilmEncoder`'s `split_reels`.

## Left to the builder (flagged as my calls)

- How to render embedded audio only: a `Player` built with a playlist (`Player(film, playlist, tolerant)`) that contains just the video-bearing contents.
- Format of the note about lost sync (`sources.txt` listing each original with its position, trim and reel).

- Exact DPX header field values (descriptor, transfer, colorimetric, packing/method bits). Validate against a reference DPX reader.
- WAV writer: reuse libsndfile if the repo already links it, else a minimal writer.
- File naming inside the layout above.
- GUI: new dialog modelled on `export_video_file_dialog`, or a new format entry in it.
- CLI flag names and docs.
- Whether to also require source resolution of at least the film container size (not decided; no resolution check in this pass).
- Tests: gate unit tests for each accepted and rejected codec/format; unit test for the DPX writer header and pixel packing; an end-to-end test exporting a short film.
