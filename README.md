DCP-o-matic is a program to generate DCPs (digital cinema packages).

Please see [dcpomatic.com](https://dcpomatic.com/) for details.

## This is the Plus

This repository is Carl Hetherington's DCP-o-matic with some improvements that Carl has not added to his own
repository yet. Each one has been sent to him as a pull request and is waiting for him. When Carl takes one, it
stops being an improvement here and is simply part of his code.

- `carl` is Carl's code exactly as it is now, with nothing of ours added.
- `main` is `carl` plus the improvements below.

The improvements, in plain words:

1. [#55](https://github.com/cth103/dcpomatic/pull/55): a 7.1 sound file now goes to the right speakers, including the
   rear ones, and the project gets 12 channels. It warns about channel layouts that cinemas do not use.
2. [#56](https://github.com/cth103/dcpomatic/pull/56): warns, before the DCP is made, when closed captions have no
   language.
3. [#57](https://github.com/cth103/dcpomatic/pull/57): fixes Atmos being written into the wrong reel when one reel
   has no Atmos.
4. [#58](https://github.com/cth103/dcpomatic/pull/58): warns when some reels of a DCP have Atmos and others do not,
   because some cinema processors then do not play the Atmos.
5. [#59](https://github.com/cth103/dcpomatic/pull/59): avoids an empty reel, and the error it gave, when a reel cut
   chosen by the user is the same as one that a referenced DCP needs.
6. [#60](https://github.com/cth103/dcpomatic/pull/60): lets a version file (VF) reuse the Atmos of an existing DCP
   instead of writing it again. Only when that Atmos covers every reel.
7. [#61](https://github.com/cth103/dcpomatic/pull/61): stops DKDMs that another DCP-o-matic program deleted from
   coming back.
8. [#62](https://github.com/cth103/dcpomatic/pull/62): adds an empty open subtitle track whenever there are closed
   captions, so that some servers do not show the closed captions on the screen. Its empty text starts 4 seconds
   into the first reel, as Bv2.1 asks.
9. [#63](https://github.com/cth103/dcpomatic/pull/63): an imported DCP with more than 8 channels maps only the
   channels it labels, so a copy of a DCP without HI and VI does not get them, nor "-HI" and "-VI" in its name.
