# DCDM export: design

## Governing principle: round trip (user decision)

The DCDM must be a faithful master: importing it back into DCP-o-matic (DPX sequence, WAVs, Atmos, subtitles, same reels) and making a DCP gives the same DCP as the original film. This drives every choice above: picture, audio and Atmos are written exactly as the final DCP would contain them, with nothing applied twice.

- Re-import must not double-process: the re-imported film uses no gain, trim, delay, audio processor or colour correction of its own, and the single audio set.
- Open risk (not decided, builder must verify): DPX files carry no XYZ flag, and DCP-o-matic normally converts imported RGB to XYZ. The re-import path needs a way to treat the DPX as already XYZ (no conversion). If that cannot be done, the round-trip test falls back to a stated tolerance and the user must be told.
- JPEG 2000 encoding is lossy: "same DCP" means same decoded picture within what the encoder gives for identical input, not necessarily identical bytes.
- Acceptance test: see Task 10 in the plan.

## Intent

Add an export that writes a Digital Cinema Distribution Master from a film: one 12-bit XYZ DPX file per frame, plus audio as one WAV file per channel. Users need an uncompressed, colour-space-correct master that other cinema tools can ingest.

## Goals

- Export a film as a DPX frame sequence, CIE XYZ, 12 bits per component.
- Audio exactly as the final DCP will sound (user decision): all PCM is rendered through the `Player`, so gain, delay, trim, fades, channel mapping and the audio processor (decoding matrix) are applied. One mono 24-bit WAV per channel. Atmos is trimmed and aligned to the picture.
- Available in the GUI (export dialog) and the CLI (`encode_cli`).
- Fits the existing `FilmEncoder` pattern, so progress, cancel and job handling work as for the other exports.

## Non-goals

- TIFF output.
- Single multichannel WAV.
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

- Audio and Atmos follow the picture (user decision, replaces earlier "originals untouched"): trim, position, gain and delay are applied so the result is exactly what the final DCP will contain.
  - PCM, standalone or embedded in a video file alike: rendered through the `Player` and the `Butler`. This applies gain, delay, trim, fades, the film's channel mapping and, when the film uses an audio processor (decoding matrix such as mid-side decoder or upmixer), the audio after that processor (user decision). Sample rate is `film->audio_frame_rate()`, resampled where needed (the `Player` already does this). 24-bit, one mono WAV per channel, per reel.
  - Dolby Atmos: written per reel through `Player::Atmos` (frames with time and `AtmosMetadata`) into a new `atmos.mxf`, using `AtmosMetadata::create` and `dcp::AtmosAsset::start_write` as `ReelWriter::write(atmos, metadata)` does. Trim and position are applied, and an Atmos content spanning several reels is cut at the reel boundaries. The MXF is re-wrapped; frame payloads are preserved. In a reel with no Atmos frames no file is written.
- Open subtitles and closed captions: written as separate files, not burnt into the DPX frames (my call; the user did not choose between this and burning). Format follows the film (Interop or SMPTE XML), with fonts. Files go in `subtitles/` (open) and `captions/` (closed). Nothing is written if the film has none.
- Consequence: the DCDM encoder must not call `set_always_burn_open_subtitles()`.
- `SubtitleFilmEncoder::text` currently drops everything but `TextType::OPEN_SUBTITLE`; closed captions need support there or in a shared helper.

## Audio layout: 5.1 or 7.1, one version only (user decision)

- Exported audio is 5.1 or 7.1, chosen from the programme, and only that one version is written. 7.1 if the rear surround channels (BSL, BSR) are in use, otherwise 5.1. No downmix and no second set.
- One directory `audio/` (per reel folder, or the output root for one reel) of mono 24-bit WAVs named by channel label: 5.1 = L, R, C, LFE, Ls, Rs; 7.1 adds Lrs, Rrs (`dcp::Channel` BSL, BSR).
- My call: a programme with fewer than six channels is padded with silent channels so the 5.1 set is complete.
- Extra channels (user decision): HI, VI and sign language (SL) are written as HI.wav, VI.wav and SL.wav when the film uses them. Lc and Rc are not written: DCP-o-matic always writes them silent in a DCP (dcp::used_audio_channels() leaves them out), so the user chose to leave them out of the DCDM too. Motion data and the Atmos sync signal are never written (the sync track is regenerated when a DCP is made).

## Reels (user decision: respect them)

- The export follows `film->reels()`. One reel: flat layout as above. More than one reel: one subdirectory per reel, `reel_01/`, `reel_02/`, ..., each holding the full layout (DPX frames or `left/` and `right/`, audio sets, `atmos.mxf` if any, subtitles, captions). Frame numbering restarts at 0 in each reel; WAVs cover exactly the reel's period.
- Audio sets, `atmos.mxf`, DPX, subtitles and captions are all per reel.
- Subtitles and captions are split per reel, like `SubtitleFilmEncoder`'s `split_reels`.

## Left to the builder (flagged as my calls)

- Check where the `Player` applies the audio processor (player.cc ~1246) so the rendered audio is post-matrix, and that gain, delay and trim are in the Butler output; add tests that prove it.

- Exact DPX header field values (descriptor, transfer, colorimetric, packing/method bits). Validate against a reference DPX reader.
- WAV writer: reuse libsndfile if the repo already links it, else a minimal writer.
- File naming inside the layout above.
- GUI: new dialog modelled on `export_video_file_dialog`, or a new format entry in it.
- CLI flag names and docs.
- Whether to also require source resolution of at least the film container size (not decided; no resolution check in this pass).
- Tests: gate unit tests for each accepted and rejected codec/format; unit test for the DPX writer header and pixel packing; an end-to-end test exporting a short film.
