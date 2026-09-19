# Session state: FreeDV text messaging fork

Fork of drowe67/freedv-gui 2.4.1 (upstream remote `upstream`, base commit
`d0377829`), branch `text-messaging`, adding a FreeDATA-inspired but *not*
wire-compatible chat feature. Six commits, roughly 5500 lines of new code.
**None of it has ever been compiled** — the machine it was written on had no
C++ toolchain.

## What exists

| Area | Files |
| --- | --- |
| Protocol core (no wx, no codec2) | `src/text_messaging/` — `FrameCodec`, `MessageStore` (SQLite), `HeardStationList`, `TextMessagingProtocol`, `TextMessagingSession` |
| Unit tests (ctest) | `src/text_messaging/test/` — frame codec, store, protocol-against-a-fake-radio |
| Modem and audio | `src/pipeline/TextMessagingModem`, `TextMessagingReceiveStep`, `TextMessagingTxQueue`, `TextMessagingTransport`, plus edits in `TxRxThread.{h,cpp}` |
| GUI | `src/gui/dialogs/dlg_text_messaging.{h,cpp}`, Tools menu entry in `topFrame.{h,cpp}`, lifecycle in `main.{h,cpp}` and `ongui.cpp` |
| Vendored | `external/sqlite3/` (amalgamation 3.50.4) |
| Docs | `doc/TEXT_MESSAGING.md` |

Design decisions already settled (do not relitigate): own protocol over codec2
DATAC13 (signalling, 30 byte payload) and DATAC4 (text, 126 byte payload), one
burst per frame; messages up to 8 fragments in one keying; ACK with 3 retries;
ping sent once; auto-reply on by default with a toggle; history in SQLite;
transmit reuses the voice-keyer PTT path.

## Next step: first build

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DUNITTEST=ON
cmake --build build -j$(nproc)
ctest --test-dir build -R text_messaging
```

Expect compile errors: every file was written without a compiler. Fix them
before judging the design. The three unit tests need no radio and no audio
device; they are the first real check that the logic works.

## Known risk areas, in the order they are likely to bite

1. `TxRxThread::transmitTextMessagingAudio_()` — the transmit path borrows the
   transmitter from the mic pipeline. Verify with two instances over a virtual
   audio cable before trusting it on a radio.
2. PTT handoff: `MainFrame::setTextMessagingPtt_()` plus
   `TextMessagingTxQueue::setOwnsTransmitter()`. A bug here means a stuck
   transmitter or voice audio leaking behind a burst.
3. codec2's realtime allocator is thread local: the modem is opened and closed
   on the GUI thread on purpose, and `rxMutex_` keeps the receive tap out of
   it during teardown. Do not move `open()`/`close()` onto another thread.
4. Payload sizes are asserted against codec2 at open time; if that check fires
   the constants in `TextMessagingTypes.h` need to follow codec2, and the
   pinned vectors in `FrameCodecTest.cpp` explain the wire format.

## Prompt to start the next session

> Read context-log.md, then get this fork building. Configure with
> `cmake -B build -DCMAKE_BUILD_TYPE=Debug -DUNITTEST=ON`, build, and work
> through the compile errors — they are in the new text messaging code, not
> upstream, so fix that code rather than weakening -Werror or dropping files
> from CMake. Ask before changing any design decision listed above. Then run
> `ctest --test-dir build -R text_messaging` (three tests, no radio or audio
> device needed); a failure there is a real bug, so fix the code unless the
> test's expectation is provably wrong. Report what built, what passed, and
> anything the compiler made look wrong. Commit build fixes separately from
> behaviour changes, and don't push.
