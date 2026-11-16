# macos-test-tools

Helpers for running DCP-o-matic's unit tests on macOS without changing the code under test. This branch shares no
history with the code branches (`carl` is a literal copy of Carl's `main`, `main` is the Plus). It is protected
against deletion.

## bcast_to_loopback

On some Macs a UDP datagram to 255.255.255.255 fails with `EHOSTUNREACH` ("No route to host") on every interface,
while subnet broadcast, unicast and `ping 255.255.255.255` work. `EncodeServerFinder` then never finds the server that
`real_encode_with_server` starts, and the test fails.

`bcast_to_loopback.c` is loaded into `unit-tests` with `DYLD_INSERT_LIBRARIES`. It interposes `sendto` and `sendmsg`
(Boost.Asio uses `sendmsg`), sends datagrams for 255.255.255.255 to 127.0.0.1 on the same port, and repeats each one
once a second for `BCAST_REPEAT_SECONDS` (default 60): the finder's first hello comes before the test's server listens,
and its next one only 10 seconds later, after the encode has finished. `BCAST_LOG=1` logs each redirect.

    lib=$(bash build.sh)     # builds build/bcast_to_loopback.dylib if missing or out of date
    DYLD_INSERT_LIBRARIES=$lib build/test/unit-tests ...
