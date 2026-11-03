# DCDM export: design

## Intent

Add an export that writes a Digital Cinema Distribution Master from a film: one 12-bit XYZ DPX file per frame, plus audio as one WAV file per channel. Users need an uncompressed, colour-space-correct master that other cinema tools can ingest.

## Goals

- Export a film as a DPX frame sequence, CIE XYZ, 12 bits per component.
- Export audio as one WAV file per channel.
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

## Left to the builder (flagged as my calls)

- Exact DPX header field values (descriptor, transfer, colorimetric, packing/method bits). Validate against a reference DPX reader.
- WAV writer: reuse libsndfile if the repo already links it, else a minimal writer.
- File naming inside the layout above.
- GUI: new dialog modelled on `export_video_file_dialog`, or a new format entry in it.
- CLI flag names and docs.
- Tests: unit test for the DPX writer header and pixel packing; an end-to-end test exporting a short film.
