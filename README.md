DCP-o-matic is a program to generate DCPs (digital cinema packages).

Please see [dcpomatic.com](https://dcpomatic.com/) for details.

## This is the Plus

This repository is Carl Hetherington's DCP-o-matic with some improvements that Carl has not added to his own
repository yet. Each one has been sent to him as a pull request and is waiting for him. When Carl takes one, it
stops being an improvement here and is simply part of his code.

- `carl` is Carl's code exactly as it is now, with nothing of ours added.
- `main` is `carl` plus the improvements below.

The improvements, in plain words. All the links are pull requests to Carl.

Subtitles, DKDMs, Atmos hints, channels and listening to HI and VI:

1. [#61](https://github.com/cth103/dcpomatic/pull/61): stops DKDMs that another DCP-o-matic program deleted from
   coming back.
2. [#62](https://github.com/cth103/dcpomatic/pull/62): adds an empty open subtitle track whenever there are closed
   captions, so that some servers do not show the closed captions on the screen. Its empty text starts 4 seconds
   into the first reel, as Bv2.1 asks.
3. [#70](https://github.com/cth103/dcpomatic/pull/70): a 7.1 sound file now goes to the right speakers, including
   the rear ones, and the project gets 12 channels.
4. [#71](https://github.com/cth103/dcpomatic/pull/71) and [#73](https://github.com/cth103/dcpomatic/pull/73): warn
   when some reels of a DCP have Atmos and others do not, because some cinema processors then do not play the Atmos.
   The second one covers a DCP whose own reels are mixed.
5. [#72](https://github.com/cth103/dcpomatic/pull/72): warns about 6 and 8 channel files whose layout is not one
   that cinemas use.
6. [#97](https://github.com/cth103/dcpomatic/pull/97): `View -> Audio` in the player and the main window lets you
   listen to the HI or VI track on its own, to check that it is the right one and in sync. Each choice is only
   enabled when the film really has that track.
7. [#99](https://github.com/cth103/dcpomatic/pull/99): a VF that refers to its OV's closed captions no longer writes
   them twice (which made an invalid DCP), and so also gets its own empty open subtitle track.

Sound MXF files added on their own (Carl has taken the first two parts as `AudioMXFContent`):

8. [#75](https://github.com/cth103/dcpomatic/pull/75): only the channels that have sound are mapped.

DCDM export (a master for archive or grading, nine pull requests):

9. [#87](https://github.com/cth103/dcpomatic/pull/87): content remembers the codec and pixel format of its source.
10. [#88](https://github.com/cth103/dcpomatic/pull/88): a writer for mono 24-bit WAV files.
11. [#89](https://github.com/cth103/dcpomatic/pull/89): a writer for 12-bit XYZ DPX images.
12. [#90](https://github.com/cth103/dcpomatic/pull/90): checks that the sources are good enough for a master.
13. [#91](https://github.com/cth103/dcpomatic/pull/91): works out the 5.1 or 7.1 audio layout of a master.
14. [#92](https://github.com/cth103/dcpomatic/pull/92): a lossless JPEG 2000 writer for the pictures.
15. [#93](https://github.com/cth103/dcpomatic/pull/93): the encoder that writes the master.
16. [#94](https://github.com/cth103/dcpomatic/pull/94): `dcpomatic2_cli --export-format dcdm` and `dcdm-dpx`.
17. [#95](https://github.com/cth103/dcpomatic/pull/95): `Jobs -> Export DCDM...` in the program.

Sign language video (the interpreter video carried on sound channel 15):

18. [#100](https://github.com/cth103/dcpomatic/pull/100): the sign language video language can only be set when the
    film has channel 15, so a DCP never declares sign language video that it does not carry.
19. [#101](https://github.com/cth103/dcpomatic/pull/101): a mapped channel 15 gets its SLVS label in the sound file,
    as Bv2.1 asks. It needs the libdcp change [libdcp#18](https://github.com/cth103/libdcp/pull/18).
