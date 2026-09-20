# Text Chat

This fork adds a keyboard chat mode to FreeDV, reachable from **Tools → Text
Chat...**. It is inspired by FreeDATA's chat feature but does not share its
protocol: two stations both need this build to talk to each other.

The name matters: **Tools → Options → Reporting** has a separate "Txt Msg"
field, which is upstream's low rate text sent alongside your voice inside the
FreeDV signal. That is a different feature with different limits, and nothing
here touches it. The source tree still calls this one text messaging
(`src/text_messaging/`, `TextMessagingDialog`), only what the operator sees
says chat.

## What the window does

* **Heard Stations** lists every station whose frames have been decoded, with
  the SNR they were last heard at and how long ago that was. Entries age out
  after an hour.
* **Ping** (enabled when a station is selected) asks that station to answer.
  Both the ping and the reply appear in the chat pane as small lines:
  `W1AW >> VK3ABC : PING!` and `VK3ABC >> W1AW : PONG! (4.0 dB, heard you at
  8.0 dB)`. A ping is sent once; if nothing comes back the window says so.
* **Chat** shows the conversation. Messages you sent are on the right with a
  light blue background; messages received are on the left, prefixed with the
  sender's callsign. Under each message is the time on the left and, for your
  own messages, a delivery chip on the right: `SENDING`, `SENT`, `RETRY #n`
  (yellow), `OK` (green) or `NO ACK` (red). `SENDING` belongs to the first
  attempt alone; once a message has been retried the chip keeps its retry
  number for the whole of that attempt, on the air and while the
  acknowledgement timer runs, so progress never appears to go backwards.
* **Send** and **Send as Broadcast** are disabled while a burst is on the air,
  so nothing is queued behind a keyed transmitter.
* The status line along the bottom says what the station is doing. A notice
  that something is queued clears itself once the transmitter keys. While an
  acknowledgement is outstanding it reads `Awaiting message ACK.` or
  `Awaiting ping ACK.`, and it keeps saying so across the retries rather than
  blinking off each time the message goes back on the air. Errors stay until
  something replaces them, which includes the station having something newer
  to say about what it is doing.
* **Send** transmits to the highlighted station and asks for confirmation. If
  no confirmation arrives within 15 seconds the message is sent again, up to
  three times, and then marked failed.
* **Send as Broadcast** clears the highlight and transmits to everybody. No
  confirmation is requested and the message is tagged `BCAST` in the window.
* **Automatically acknowledge messages and answer pings** controls whether
  this station transmits by itself. Leave it off if you may not transmit
  unattended; incoming messages are still displayed.

Enter sends; Shift+Enter starts a new line. Your callsign comes from the
reporting callsign in **Tools → Options**.

Chat history and heard stations are kept in `text_messaging.db` in FreeDV's
user data directory, and history older than 30 days is dropped at startup.

Setting `FREEDV_TEXT_CHAT_UI_LOG` in the environment makes the window log what
it is showing: when it was created and became able to receive updates, every
message it adds, the delivery chip text after each change, and any status
change that arrived for a message not currently in the view. That last one is
the silent failure behind a chip that never updates. `test/test_text_chat_loopback.sh`
sets it for both stations.

## How it works on the air

Text messaging does not travel inside RADE. It uses the codec2 raw data
modes, sent as short bursts in their own keying of the transmitter:

| Traffic | Mode | Payload after the modem's CRC |
| --- | --- | --- |
| Ping, pong, acknowledgement | DATAC13 | 14 bytes |
| Message text | DATAC4 | 54 bytes |

These are what codec2 actually hands us per modem frame, less the two byte CRC
the raw data API appends. `TextMessagingModem` checks them against the modem
when it opens and refuses to start if they have drifted, rather than
truncating frames on the air; text messaging then reports itself unavailable
and the Tools menu entry stays greyed out.

Every frame is sent as a complete burst (preamble, frame, postamble) with a
100 ms gap after it, so the receiving modem acquires each frame on its own
rather than having to hold sync across a whole message. A message longer than
one frame is split into up to eight fragments, all sent in one keying; 312
characters is the limit. DATAC4 is slow on purpose, so a full length message
holds the transmitter for roughly half a minute: if you use FreeDV's transmit
time-out timer, set it longer than that or it will cut a long message off.

A DATAC13 frame is only 14 bytes, so the two traffic classes do not share one
header layout. Both start with the same 12 bytes: frame type, destination
callsign CRC-24, the origin callsign packed into six bytes (base 40, up to
nine characters, so `VK3ABC/P` fits), and a message ID used to match
acknowledgements. Text frames then add a fragment index and count; signalling
frames are always a single fragment and spend those two bytes on payload
instead, which is what lets a ping fit in DATAC13 at all. Both end with a
payload length byte, which tells the decoder where the payload stops and the
zero padding out to the modem frame size begins.

That leaves a 13 byte header and one payload byte in a signalling frame --
exactly enough for the SNR a pong reports -- and a 15 byte header with 39
bytes of text in a DATAC4 frame.

The origin callsign CRC-24 is not sent. It is the CRC of the callsign already
in the frame, and three bytes is a fifth of a DATAC13 frame. The modem's own
CRC-16 protects each frame, so nothing is added for that.

Receiving runs continuously: the DATAC13 and DATAC4 demodulators sit on a tap
off the receive audio, on their own thread, and do not touch voice decoding.

## Transmitting

* Voice always wins. A queued message waits until you are not transmitting.
* Only one transmission is outstanding at a time. An acknowledgement being
  sent jumps ahead of anything else queued, because the far end is waiting on
  a timer.
* A message that arrives twice (because our acknowledgement was lost) is
  acknowledged again and shown once.
* Sending needs a transmit sound device, which means the two sound card
  configuration. Receiving works either way.

Automatic acknowledgements and pongs key the transmitter without you touching
anything, which in most countries makes this an automatically controlled
station (in the United States, FCC 97.221). Operating within those rules is
your responsibility; the checkbox in the window turns automatic transmission
off.
