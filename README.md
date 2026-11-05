DCP-o-matic is a program to generate DCPs (digital cinema packages).

Please see [dcpomatic.com](https://dcpomatic.com/) for details.

## This is the Plus

This repository is Carl Hetherington's DCP-o-matic with some improvements that Carl has not added to his own
repository yet. Each one has been sent to him as a pull request and is waiting for him. When Carl takes one, it
stops being an improvement here and is simply part of his code.

- `carl` is Carl's code exactly as it is now, with nothing of ours added.
- `main` is `carl` plus the improvements below.

The improvements, in plain words. All the links are pull requests to Carl.

Atmos, subtitles, DKDMs and channels:

1. [#60](https://github.com/cth103/dcpomatic/pull/60): lets a version file (VF) reuse the Atmos of an existing DCP
   instead of writing it again. Only when that Atmos covers every reel.
2. [#61](https://github.com/cth103/dcpomatic/pull/61): stops DKDMs that another DCP-o-matic program deleted from
   coming back.
3. [#62](https://github.com/cth103/dcpomatic/pull/62): adds an empty open subtitle track whenever there are closed
   captions, so that some servers do not show the closed captions on the screen. Its empty text starts 4 seconds
   into the first reel, as Bv2.1 asks.
4. [#65](https://github.com/cth103/dcpomatic/pull/65): a picture MXF whose video is ignored is no longer read frame
   by frame.
5. [#66](https://github.com/cth103/dcpomatic/pull/66): a decoder told to ignore Atmos no longer sends Atmos data.
6. [#67](https://github.com/cth103/dcpomatic/pull/67): adds `Content::untrimmed_clone()`, needed by the pull requests
   below.
7. [#70](https://github.com/cth103/dcpomatic/pull/70): a 7.1 sound file now goes to the right speakers, including
   the rear ones, and the project gets 12 channels.
8. [#71](https://github.com/cth103/dcpomatic/pull/71) and [#73](https://github.com/cth103/dcpomatic/pull/73): warn
   when some reels of a DCP have Atmos and others do not, because some cinema processors then do not play the Atmos.
   The second one covers a DCP whose own reels are mixed.
9. [#72](https://github.com/cth103/dcpomatic/pull/72): warns about 6 and 8 channel files whose layout is not one
    that cinemas use.

Sound MXF files added on their own (a series, in this order):

10. [#68](https://github.com/cth103/dcpomatic/pull/68): a PCM sound MXF can be added as content.
11. [#74](https://github.com/cth103/dcpomatic/pull/74): the DCP player converts PCM in the same place.
12. [#75](https://github.com/cth103/dcpomatic/pull/75): only the channels that have sound are mapped.
13. [#76](https://github.com/cth103/dcpomatic/pull/76): the MXF goes into the DCP as it is when nothing changes its
    sound.
14. [#77](https://github.com/cth103/dcpomatic/pull/77): the head and tail of the MXF stay at the start and end of a
    reel.
15. [#78](https://github.com/cth103/dcpomatic/pull/78): one MXF can span several reels of the film.
16. [#79](https://github.com/cth103/dcpomatic/pull/79): a trimmed WAV keeps its whole length in the DCP's sound
    asset.
17. [#80](https://github.com/cth103/dcpomatic/pull/80): a 2D picture MXF goes into the DCP as it is when nothing
    changes its image.
18. [#81](https://github.com/cth103/dcpomatic/pull/81): a trimmed picture MXF that is written again keeps its whole
    length.
19. [#82](https://github.com/cth103/dcpomatic/pull/82): a picture MXF made of several whole reels, changed in the
    same way, is written once.
20. [#83](https://github.com/cth103/dcpomatic/pull/83): a WAV made of several whole reels with the same settings is
    written once.
21. [#84](https://github.com/cth103/dcpomatic/pull/84): WAVs that play together with the same trims keep their
    whole length.
22. [#85](https://github.com/cth103/dcpomatic/pull/85): a sound or picture MXF is not copied into the DCP again when
    a copy is already there.
23. [#86](https://github.com/cth103/dcpomatic/pull/86): an Atmos MXF goes into the DCP as it is when nothing stops
    it.

DCDM export (a master for archive or grading, nine pull requests):

24. [#87](https://github.com/cth103/dcpomatic/pull/87): content remembers the codec and pixel format of its source.
25. [#88](https://github.com/cth103/dcpomatic/pull/88): a writer for mono 24-bit WAV files.
26. [#89](https://github.com/cth103/dcpomatic/pull/89): a writer for 12-bit XYZ DPX images.
27. [#90](https://github.com/cth103/dcpomatic/pull/90): checks that the sources are good enough for a master.
28. [#91](https://github.com/cth103/dcpomatic/pull/91): works out the 5.1 or 7.1 audio layout of a master.
29. [#92](https://github.com/cth103/dcpomatic/pull/92): a lossless JPEG 2000 writer for the pictures.
30. [#93](https://github.com/cth103/dcpomatic/pull/93): the encoder that writes the master.
31. [#94](https://github.com/cth103/dcpomatic/pull/94): `dcpomatic2_cli --export-format dcdm` and `dcdm-dpx`.
32. [#95](https://github.com/cth103/dcpomatic/pull/95): `Jobs -> Export DCDM...` in the program.
