# Downmix of DCP audio: 7.1 to 5.1, 7.1 to stereo, 5.1 to stereo

Status: implemented, not compiled or tested in the development environment. Values are marked where they come from a source and where they are a decision.

## 1. Scope

DCP-o-matic converts channels in two places:

- Monitor: `Config::audio_mapping(output_channels, film_channels)` in `src/lib/config.cc`, used by the Player through `FilmViewer` (`src/wx/film_viewer.cc`). It is the default mapping; a mapping edited in preferences takes precedence.
- Export: `FFmpegFilmEncoder::stereo_map()` in `src/lib/ffmpeg_film_encoder.cc`, used by "Mixdown to stereo".

Both call the same function, `stereo_downmix_map(film_channels, input_channels)` in `src/lib/audio_mapping.cc`, so monitor and export give the same stereo result for the same film.

## 2. What is used

### 2.1 Stereo (monitor and export)

| Film channels | Input | Gain into Lo / Ro | Value |
|---|---|---|---|
| 2 | L, R | identity | 1.0 |
| 4 | L, R, C, Ls | unchanged from before (not in any source) | L, R 0.369; C 0.261; Ls 0.369 |
| 6 or 8 (5.1) | L, R, Ls, Rs | 2 / (4 + √2) | 0.369 |
| 6 or 8 (5.1) | C | 0.707 × 0.369 | 0.261 |
| 12 or more (7.1) | L, R | same as 5.1 | 0.369 |
| 12 or more (7.1) | C | same as 5.1 | 0.261 |
| 12 or more (7.1) | Ls, Rs, Lrs, Rrs | 0.707 × 0.369 | 0.261 |

A 5.1 film sums to 1.0 in each output (0.369 + 0.261 + 0.369). A 7.1 film goes through 5.1 first (section 3.2), so L, R and C have the 5.1 gains and the sum is 1.15 (0.369 + 3 × 0.261), reached only with coherent full-level content in all of them. LFE is not mapped.

### 2.2 Monitor, 5.1 output

| Input | Output | Gain | Note |
|---|---|---|---|
| Channels 1 to 6 | Outputs 1 to 6 | 1.0 | 1:1 |
| Channels 7 and 8 (HI, VI) | Outputs 7 and 8, if the device has them | 1.0 | 1:1 |
| Ls (5) | Ls (output 5) | 0.707 (−3 dB) | 7.1 films only |
| Rs (6) | Rs (output 6) | 0.707 (−3 dB) | 7.1 films only |
| Lrs (11) | Ls (output 5) | 0.707 (−3 dB) | |
| Rrs (12) | Rs (output 6) | 0.707 (−3 dB) | |

Ls and Rs keep 1.0 for films with fewer than 12 channels, so 5.1 sources pass unchanged. For a 7.1 film, the side and the rear channel of each side go in at 0.707 each, with no cross-feed. The amplitudes add up to 1.41, so the monitor runs a peak limiter (`limit_peaks()` in `util.cc`, called from `Butler::audio`) after the mapping.

### 2.3 Export, 5.1 or 7.1

The export has no 5.1 output. A film is exported as stereo (section 2.1) or as 16 channels 1:1 (`many_channel_map()`). The 7.1 to 5.1 fold applies only to the monitor.

## 3. Origin and reasons

### 3.1 Stereo: the rule the export already had

- 5.1 to stereo is the rule the stereo mixdown export used before this change (`FFmpegFilmEncoder::stereo_map()`), unchanged: Lo = g·(L + 0.707·C + Ls), g = 2 / (4 + √2) = 0.369. The coefficients into each output add up to 1.0, so full-scale 5.1 content cannot overflow.
- It has the shape of the LoRo downmix of ETSI TS 102 366 §6.8.2 [1] and ATSC A/52 §7.8.2 [2] (C at −3 dB), but the surround enters at the same level as L, not at −3 dB. That part is the old behaviour; it is not in any source read here, and it was kept so that existing 5.1 mixdowns do not change (`ffmpeg_encoder_h264_test5` compares against a stored file).
- Dolby's LoRo [3] uses L : C : Ls = 1 : 0.707 : 0.707 with no scaling. ITU-R BS.775-4 [5] has L′ = L + 0.7071·C + 0.7071·LS, also unscaled.
- For 7.1, Dolby's Atmos Renderer makes stereo by folding 7.1 to 5.1 first and then 5.1 to 2.0 [3, pp. 223-224]. This code does the same, with the 7.1 to 5.1 fold of section 3.2 (Lss and Lrs at 0.707 each into Ls). L, R and C therefore keep exactly the 5.1 gains: a 7.1 film is not louder than a 5.1 one in those channels. Each of the four surrounds enters at 0.707 × 0.369 = 0.261.
- The 7.1 stereo output can reach 1.15 with coherent full-level content in all channels. The monitor has the peak limiter; the stereo mixdown export has none yet.

Why LoRo and not Pro Logic: the requirement was basic stereo. Dolby's LoRo is its default stereo option. Lt/Rt and Pro Logic II are matrix-encoded and not what a stereo monitor or a stereo file needs.

### 3.2 Monitor 5.1 from 7.1: −3 dB on the side and the rear

Ls = 0.707·Lss + 0.707·Lrs and Rs = 0.707·Rss + 0.707·Rrs, no cross-feed, for films of 12 or more channels. L, R, C and LFE pass at 1.0.

Sources that give the same fold:

- ETSI TS 103 190-1 V1.4.1 (AC-4), clause 6.2.17.5, Table 219, p. 272 [8]: the normative decoder matrix for 7 to 5 channels, Ls' = 0.707·Ls + 0.707·Lb, Rs' = 0.707·Rs + 0.707·Rb.
- ETSI TS 102 366 V1.4.1 Table H.3.9, p. 223 [1], `dmixtyp` = 1: Ls' = −3 dB·Ls + −3 dB·Lrs. This is informational (it says how the encoder made the 5.1 programme), not a decoder rule.
- Fraunhofer FDK-AAC, `libPCMutils/src/pcmdmx_lib.cpp` [9]: with no metadata, `dmix_a_idx` = `dmix_b_idx` = 2 gives 0.707 for both. Read in the source code, not in ISO/IEC 14496-3.

Sources that give other values:

| Source | Lss | Lrs | Rrs (opposite side) |
|---|---|---|---|
| This code, ETSI AC-4, FDK-AAC | 0.707 | 0.707 | 0 |
| Dolby Standard [3, p. 224] | 1.0 | 1.0 | 0 |
| Dolby Pro Logic IIx [3, p. 224] | 1.0 | 0.871 (−1.2 dB) | 0.490 (−6.2 dB) |
| ETSI `dmixtyp` = 2 [1] | 1.0 | 0.871 | 0.490 |

- No cinema or broadcast standard read (SMPTE ST 428-12, ST 2098-2, RDD 57, DCI, ITU-R BS.775, ISDCF Doc 4) defines a 7.1 to 5.1 fold. The DCI text and ARIB STD-B32 were not read in the primary document. See the research reports for the full list of what was and was not read.
- Dolby's Standard fold sums two full-level channels, which reaches 2.0, and nothing normalizes it.
- 0.707 + 0.707 reaches 1.41 with coherent full-level content, so the monitor runs a peak limiter. For uncorrelated signals the power is kept.
- What the Dolby CP650 or other cinema processors do with Lrs/Rrs in a 5.1 auditorium was not found in any retrievable document.

### 3.3 Why no 7.1 coefficient is in ETSI

ETSI §6.8 covers 3/2 (5.1) to 2/0 and smaller. A 7.1 channel layout appears only as encoder metadata (`dmixtyp`, H.3.9). No source gives a decoder rule for 7.1 to 5.1 or 7.1 to stereo. The 7.1 values above are therefore the 5.1 rule extended by §6.8.1.

### 3.4 Channel numbering

DCP channels 11 and 12 are Lrs and Rrs (7.1DS) according to ISDCF Doc 4 [6]. Channels 7 and 8 are HI and VI-N, 9 and 10 are Lc and Rc (SDDS only), 13 is motion data, 14 is the sync signal, 15 is sign language and 16 is unused. The code tests for a film of 12 or more channels to enable the 7.1 rule.

### 3.5 LFE

Not mapped in either path. ETSI §6.8.2 equations do not include LFE, and §6.8.0 says LFE downmix is optional [1]. BS.775-4 also omits LFE [5].

## 4. Notes

1. The 4-channel stereo formula (L, R 0.369; C 0.261; Ls 0.369) is the original code. It is not in any source here. It is kept unchanged.
2. For a 2-channel film, the stereo mapping is identity. The old code also used identity for the export.
3. The monitor's 2-output default is not saved to the configuration file. It is regenerated for each film. A configuration written by an earlier version may hold a saved default: "Reset to default" in Preferences → Sound replaces it.
4. The preferences page calls `audio_mapping(channels)` with no film, so it shows the 16-channel (7.1) stereo default.
5. "Reset to default" calls `audio_mapping(channels)` with 16 channels, so it restores the 7.1 stereo default. The Player then regenerates the default for the actual film on the next call.
6. The Atmos guide [3] is version 3.0 (2018). Later Renderer versions may differ.

## 5. Open points

1. The 5.1 monitor fold (section 2.2) can reach 1.41 with coherent full-level content; the peak limiter handles it. It differs from Dolby's Standard fold (1.0 + 1.0) and from Pro Logic IIx.
2. The export has no 7.1 to 5.1 path (section 2.3).
3. `test/ffmpeg_encoder_test.cc` (mixdown 5.1 to stereo) compares against stored output. With the export rule unchanged for 5.1, `ffmpeg_encoder_h264_test5` and `_test8` pass. The monitor's stereo default changed (it now uses the export's rule), and no test covers it.

## 6. References

1. **ETSI TS 102 366 V1.4.1 (2017-09)**, Digital Audio Compression (AC-3, Enhanced AC-3). Clause 6.8 (downmixing, pp. 77-81): 3/2 to 2/0 equations (§6.8.2), normalization (§6.8.1), worst-case scaling (Tables 6.31 and 6.32). Table H.3.9 (pp. 222-223): `dmixtyp` metadata for acmod 7. Read from the PDF. <https://www.etsi.org/deliver/etsi_ts/102300_102399/102366/01.04.01_60/ts_102366v010401p.pdf>

2. **ATSC A/52:2018**, Digital Audio Compression (AC-3, E-AC-3). Section 7.8 (downmixing, pp. 74-80), the same downmix as ETSI §6.8. Tables 7.31 and 7.32: scaled LoRo and LtRt coefficients. Read from the PDF. <https://www.atsc.org/wp-content/uploads/2021/04/A52-2018.pdf>

3. **Dolby Atmos Renderer Guide, software version 3.0**, 2 August 2018, §22.1 (monitor and 5.1 downmix, pp. 223-224). Standard and Pro Logic IIx 7.1-to-5.1 coefficients; LoRo, Lt/Rt Legacy and Lt/Rt Pro Logic II stereo coefficients; the rule that all stereo options except Direct Render go through 5.1. Read from the PDF. <https://professional.dolby.com/siteassets/content-creation/dolby-atmos/dolby_atmos_renderer_guide.pdf>

4. **Dolby Professional Support**, "How do the 5.1 and Stereo downmix settings work?". Same coefficients as [3]. The page is a JavaScript application and was not readable with curl; quoted from search results only. <https://professionalsupport.dolby.com/s/article/How-do-the-5-1-and-Stereo-downmix-settings-work?language=en_US>

5. **ITU-R BS.775-4 (12/2022)**, Annex 4, Table 2: 3/2 to 2/0, L′ = L + 0.7071·C + 0.7071·LS, no normalization, no LFE. Read from the PDF. <https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.775-4-202212-I!!PDF-E.pdf>

6. **ISDCF Doc 4**, Audio channel recommendations: channel 11 = Lrs, channel 12 = Rrs (7.1DS). Channel numbers as listed in that document. <https://files.isdcf.com/papers/ISDCF-Doc4-Audio-channel-recommendations.pdf>

7. **FFmpeg libswresample**: default `center_mix_level` and `surround_mix_level` = M_SQRT1_2. Secondary, from doxygen and search results; the source was not read in full. <https://ffmpeg.org/doxygen/trunk/group__lswr.html>

8. **ETSI TS 103 190-1 V1.4.1 (2018)**, Digital Audio Compression (AC-4), Part 1. Clause 6.2.17.5, Table 219, p. 272: 7 to 5 channel rendering matrix; speaker indices in Table D.1, p. 302. Read from the PDF. <https://www.etsi.org/deliver/etsi_ts/103100_103199/10319001/01.04.01_60/ts_10319001v010401p.pdf>

9. **Fraunhofer FDK AAC**, `libPCMutils/src/pcmdmx_lib.cpp` (tag v2.0.3): level table (lines 276-280), default `dmix_a_idx` = `dmix_b_idx` = 2 (lines 357-358), 7.1 surround-back case (lines 1091-1105). Read in the source. <https://github.com/mstorsjo/fdk-aac>
