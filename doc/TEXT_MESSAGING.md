# Text Messaging

This fork adds a keyboard chat mode to FreeDV, reachable from **Tools → Text
Messaging...**. It is inspired by FreeDATA's chat feature but does not share
its protocol: two stations both need this build to talk to each other.

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
  own messages, a delivery chip on the right: `SENDING`, `SENT`, `RETRY n`
  (yellow), `OK` (green) or `FAILED` (red).
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

## How it works on the air

Text messaging does not travel inside RADE. It uses the codec2 raw data
modes, sent as short bursts in their own keying of the transmitter:

| Traffic | Mode | Payload |
| --- | --- | --- |
| Ping, pong, acknowledgement | DATAC13 | 30 bytes |
| Message text | DATAC4 | 126 bytes |

Every frame is sent as a complete burst (preamble, frame, postamble) with a
100 ms gap after it, so the receiving modem acquires each frame on its own
rather than having to hold sync across a whole message. A message longer than
one frame is split into up to eight fragments, all sent in one keying; 864
characters is the limit. DATAC4 is slow on purpose, so a full length message
holds the transmitter for roughly half a minute: if you use FreeDV's transmit
time-out timer, set it longer than that or it will cut a long message off.

Each frame carries an 18 byte header: frame type, destination callsign CRC-24,
origin callsign CRC-24, the origin callsign packed into six bytes (base 40,
up to nine characters, so `VK3ABC/P` fits), a message ID used to match
acknowledgements, the fragment index and count, and the payload length. The
modem's own CRC-16 protects each frame, so nothing is added for that.

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
