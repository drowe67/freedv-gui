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
- Carrier sense (`826d49ff`) was proven on the bench on 2026-09-21: a 22 s
  four-fragment burst held the other station busy throughout, including the
  gaps between fragments, and a ping queued mid-burst waited for the clear.
- The chat window now has one send button (`>> Broadcast` / `>> CALL`), a
  station context menu, and an Add Station box; all confirmed on the bench.

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

## Bench status as of 2026-09-21 evening (five runs)

Fixed and confirmed on the bench: prompt replies, retry backoff, the
answered station going first, fragment reservation (any addressee), the
random pause when the channel clears, the receiver reset after our own burst
(a frame was lost on a clean channel without it), the busy hold at 2 s, the
main window no longer stealing focus on a chat changeover, and a burst being
dropped when XMIT is pressed during it.

Still open:

1. The transmit thread fails to confirm about one burst in fifteen; the
   transport releases it one second late by the playout margin. Confirmation
   depends on the transmit thread seeing the output buffer drain, and that
   thread is paced by microphone input, which on the bench is a silent
   monitor source. Measure on a real sound device before changing anything.
2. The answered station always goes first, so a long queue on one side holds
   the channel; the other station's retry waited 80 s in run 4. Possible
   rule: the acknowledging station gets a turn after a few consecutive ACKs.
3. Same-second keying is down to about one in thirty. The last case was a
   station re-keying after a broadcast; the post-transmit turnaround is now
   3.5 s to put the listener first. Not yet seen on the bench after that.

## Running the bench from a tmux shell

The shell has no X variables. Export `DISPLAY=:0.0` and the `XAUTHORITY` that
`tmux show-environment` reports, then `FREEDV_TEST_MODE=4 test/test_text_chat_loopback.sh up`.
