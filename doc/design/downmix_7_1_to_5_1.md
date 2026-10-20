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
| 6 or 8 (5.1) | L, R | 1/(1 + √2) | 0.414 |
| 6 or 8 (5.1) | C | 0.707 / (1 + √2) | 0.293 |
| 6 or 8 (5.1) | Ls, Rs | 0.707 / (1 + √2) | 0.293 |
| 12 or more (7.1) | L, R | 1/(1 + 3/√2) | 0.320 |
| 12 or more (7.1) | C | 0.707 × 0.320 | 0.227 |
| 12 or more (7.1) | Ls, Rs, Lrs, Rrs | 0.707 × 0.320 | 0.227 |

Each output sums to 1.0 (5.1: 0.414 + 0.293 + 0.293; 7.1: 0.320 + 3 × 0.227). LFE is not mapped.

### 2.2 Monitor, 5.1 output

| Input | Output | Gain | Note |
|---|---|---|---|
| Channels 1 to 6 | Outputs 1 to 6 | 1.0 | 1:1 |
| Channels 7 and 8 (HI, VI) | Outputs 7 and 8, if the device has them | 1.0 | 1:1 |
| Ls (5) | Ls (output 5) | 0.87 (−1.2 dB) | 7.1 films only |
| Rs (6) | Rs (output 6) | 0.87 (−1.2 dB) | 7.1 films only |
| Lrs (11) | Ls (output 5) | 0.49 (−6.2 dB) | |
| Rrs (12) | Rs (output 6) | 0.49 (−6.2 dB) | |

Ls and Rs keep 1.0 for films with fewer than 12 channels, so 5.1 sources pass unchanged. For a 7.1 film, the side channel weighs 0.87 and the rear 0.49. Their squares add up to 1.0 (power kept for uncorrelated signals), but the amplitudes add up to 1.36, so the monitor runs a peak limiter (`limit_peaks()` in `util.cc`, called from `Butler::audio`) after the mapping.

### 2.3 Export, 5.1 or 7.1

The export has no 5.1 output. A film is exported as stereo (section 2.1) or as 16 channels 1:1 (`many_channel_map()`). The 7.1 to 5.1 fold applies only to the monitor.

## 3. Origin and reasons

### 3.1 Stereo: ETSI normalization over Dolby's LoRo

- The stereo equations are the LoRo downmix of ETSI TS 102 366 §6.8.2 [1] and ATSC A/52 §7.8.2 [2]: Lo = L + clev·C + slev·Ls, with clev = slev = 0.707 (−3 dB).
- The scaling comes from ETSI §6.8.1 [1]: all coefficients into an output are attenuated equally so that their sum does not exceed 1. This prevents overflow when all channels are at full scale. The worst case for L0R0 is 1/2.414 = 0.414 (Table 6.31); for 7.1 the same rule gives 1/3.121 = 0.320.
- Dolby's LoRo [3] uses the same ratio (L : C : Ls = 1 : 0.707 : 0.707) but no scaling. For 7.1, Dolby's chain is: Lrs and Rrs into Ls and Rs at 0 dB, then LoRo. After the chain, Ls contributes 0.707 and Lrs contributes 0.707. Scaling that chain by the same rule gives L 0.320, C 0.227, Ls 0.227, Lrs 0.227, which is the table above.
- So the values are Dolby's stereo ratio with ETSI's normalization factor.

Why LoRo and not Pro Logic: the requirement was basic stereo. Dolby's LoRo is its default stereo option. Lt/Rt and Pro Logic II are matrix-encoded and not what a stereo monitor or a stereo file needs.

Why normalize: Dolby's LoRo has no scaling, so full-scale 5.1 content can reach 2.41 and full-scale 7.1 content 3.12. The normalization keeps the output within 1.0.

### 3.2 Monitor 5.1 from 7.1: Dolby Standard at 0 dB

- Dolby's Standard 7.1-to-5.1 fold is Ls = 0 dB·Lss + 0 dB·Lrs, Rs = 0 dB·Rss + 0 dB·Rrs [3, p. 224].
- Dolby's Standard sums two full-level channels, which reaches 2.0. The decision here: side 0.87 and rear 0.49, the numbers of Pro Logic IIx (−1.2 dB and −6.2 dB) used here for same-side channels only, with no cross-feed. Their squares add up to 1.0. A peak limiter handles full-level coherent content. No source gives this exact fold; it is ours.
- The alternative in the same guide, Pro Logic IIx (Ls = Lss + (−1.2 dB)·Lrs + (−6.2 dB)·Rrs) [3], was not chosen because it is not basic 5.1 and because it does not match the cinema decision.
- The ETSI option `dmixtyp = 2` (Table H.3.9, pp. 222-223 [1]) has the same −1.2 / −6.2 dB form, but it is encoder metadata, not a decoder rule.

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
7. ATSC A/52 §7.8.2 [2] says "scaled (by 0.453)" for LoRo, but its own table gives 0.4143. The table value is used.

## 5. Open points

1. The 5.1 monitor fold (section 2.2) uses weights 0.87 (side) and 0.49 (rear), which are our choice and not Dolby's. −3 dB (0.707) keeps the power of uncorrelated signals but can reach 1.41.
2. The export has no 7.1 to 5.1 path (section 2.3).
3. `test/ffmpeg_encoder_test.cc` (mixdown 5.1 to stereo) may compare against stored output. The values changed, so it may fail. Run it on a machine with the build dependencies.

## 6. References

1. **ETSI TS 102 366 V1.4.1 (2017-09)**, Digital Audio Compression (AC-3, Enhanced AC-3). Clause 6.8 (downmixing, pp. 77-81): 3/2 to 2/0 equations (§6.8.2), normalization (§6.8.1), worst-case scaling (Tables 6.31 and 6.32). Table H.3.9 (pp. 222-223): `dmixtyp` metadata for acmod 7. Read from the PDF. <https://www.etsi.org/deliver/etsi_ts/102300_102399/102366/01.04.01_60/ts_102366v010401p.pdf>

2. **ATSC A/52:2018**, Digital Audio Compression (AC-3, E-AC-3). Section 7.8 (downmixing, pp. 74-80), the same downmix as ETSI §6.8. Tables 7.31 and 7.32: scaled LoRo and LtRt coefficients. Read from the PDF. <https://www.atsc.org/wp-content/uploads/2021/04/A52-2018.pdf>

3. **Dolby Atmos Renderer Guide, software version 3.0**, 2 August 2018, §22.1 (monitor and 5.1 downmix, pp. 223-224). Standard and Pro Logic IIx 7.1-to-5.1 coefficients; LoRo, Lt/Rt Legacy and Lt/Rt Pro Logic II stereo coefficients; the rule that all stereo options except Direct Render go through 5.1. Read from the PDF. <https://professional.dolby.com/siteassets/content-creation/dolby-atmos/dolby_atmos_renderer_guide.pdf>

4. **Dolby Professional Support**, "How do the 5.1 and Stereo downmix settings work?". Same coefficients as [3]. The page is a JavaScript application and was not readable with curl; quoted from search results only. <https://professionalsupport.dolby.com/s/article/How-do-the-5-1-and-Stereo-downmix-settings-work?language=en_US>

5. **ITU-R BS.775-4 (12/2022)**, Annex 4, Table 2: 3/2 to 2/0, L′ = L + 0.7071·C + 0.7071·LS, no normalization, no LFE. Read from the PDF. <https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.775-4-202212-I!!PDF-E.pdf>

6. **ISDCF Doc 4**, Audio channel recommendations: channel 11 = Lrs, channel 12 = Rrs (7.1DS). Downloaded; the table was taken from the note in `doc/design/atmos_sync/standards_channel14.md`. <https://files.isdcf.com/papers/ISDCF-Doc4-Audio-channel-recommendations.pdf>

7. **FFmpeg libswresample**: default `center_mix_level` and `surround_mix_level` = M_SQRT1_2. Secondary, from doxygen and search results; the source was not read in full. <https://ffmpeg.org/doxygen/trunk/group__lswr.html>
