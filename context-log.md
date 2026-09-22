# Session state: FreeDV text messaging fork

Fork of drowe67/freedv-gui 2.4.1 (upstream remote `upstream`, base commit
`d0377829`), branch `text-messaging`, adding a FreeDATA-inspired but *not*
wire-compatible chat feature. 35 commits on top of upstream as of 2026-09-21.

## Where things stand

- Builds clean (`build/`, Debug, `-DUNITTEST=ON`). Binary is current with HEAD.
- All three ctest units pass: frame codec, store, protocol-against-a-fake-radio.
- Two-instance loopback bench exists: `test/test_text_chat_loopback.sh up|down|status`.
  It has caught and confirmed fixes for PTT handoff, burst timing, the missed
  transmit flag, turnaround windows, and the status line (commits of 2026-09-20).
- HEAD (`826d49ff`, carrier sense from the demodulator's FREEDV_RX_SYNC) was
  written last and **no bench log survives showing it exercised**. Treat it as
  unit-tested only until a loopback run shows a `busy` spell in
  `FREEDV_TEXT_CHAT_TX_LOG` output while the other station is mid-burst.

## What exists

| Area | Files |
| --- | --- |
| Protocol core (no wx, no codec2) | `src/text_messaging/` — `FrameCodec`, `MessageStore` (SQLite), `HeardStationList`, `TextMessagingProtocol`, `TextMessagingSession` |
| Unit tests (ctest) | `src/text_messaging/test/` |
| Modem and audio | `src/pipeline/TextMessagingModem`, `TextMessagingReceiveStep`, `TextMessagingTxQueue`, `TextMessagingTransport`, plus edits in `TxRxThread.{h,cpp}` |
| GUI | `src/gui/dialogs/dlg_text_messaging.{h,cpp}`, Tools menu entry in `topFrame.{h,cpp}`, lifecycle in `main.{h,cpp}` and `ongui.cpp` |
| Bench | `test/test_text_chat_loopback.sh`, `test/freedv-text-chat-station.conf.tmpl` |
| Vendored | `external/sqlite3/` (amalgamation 3.50.4) |
| Docs | `doc/TEXT_MESSAGING.md` |

Design decisions already settled (do not relitigate): own protocol over codec2
DATAC13 (signalling) and DATAC4 (text), one burst per frame; messages up to 8
fragments in one keying; ACK with 3 retries; ping sent once; auto-reply on by
default with a toggle; history in SQLite; transmit reuses the voice-keyer PTT
path; carrier sense holds the queue for CHANNEL_BUSY_HOLD_MILLISECONDS after
sync was last seen, and a receiver locked for over a minute is ignored.

## Bench traps (cost a session each; see memory too)

1. Each station needs its own `HOME`, or both open the same chat database.
2. Reporting is never disabled by config; the bench points the reporter host at
   127.0.0.1. Confirm with `ss -tnp` that neither PID has an external socket.
3. Text chat cannot transmit until Start is pressed in **both** windows.
4. Use `Hamlib Dummy` for PTT. `FREEDV_TEST_MODE=4` drops both to 700D so CPU
   starvation is not mistaken for a protocol bug.
5. `down` removes the bench workdir and its logs. Copy logs out first if a run
   showed something worth keeping.

## Next step: prove carrier sense on the bench

```sh
cmake --build build -j$(nproc)
test/test_text_chat_loopback.sh up      # then press Start in both windows
```

Send a long (multi-fragment) message from A, and while it is still keyed queue
a message from B. Expected in stationB's `freedv.log`: a busy spell logged by
the transport, B's burst held until A's fragments end plus the hold, then B
keys. Failure modes to look for: B keying between A's fragments (hold too
short), B never keying (sync stuck; the one-minute ignore should clear it), or
a busy spell logged while nothing is on the air (false trigger on the null
sink's silence).

Then `test/test_text_chat_loopback.sh down`.
