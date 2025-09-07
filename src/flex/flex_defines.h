#ifndef FLEX_DEFINES_H
#define FLEX_DEFINES_H

#define FLEX_TCP_PORT (4992)
#define FLEX_SAMPLE_RATE (24000)
#define MAX_VITA_PACKETS (200)
#define MAX_VITA_SAMPLES (180) /* 7.5ms/block @ 24000 Hz */
#define VITA_SAMPLES_TO_SEND MAX_VITA_SAMPLES
#define MIN_VITA_PACKETS_TO_SEND (1)
#define MAX_VITA_PACKETS_TO_SEND (10)
#define US_OF_AUDIO_PER_VITA_PACKET (5250)
#define VITA_IO_TIME_INTERVAL_US (US_OF_AUDIO_PER_VITA_PACKET * MIN_VITA_PACKETS_TO_SEND) /* Time interval between subsequent sends or receives */
#define MAX_JITTER_US (500) /* Corresponds to +/- the maximum amount VITA_IO_TIME_INTERVAL_US should vary by. */
#define US_TO_MS (1000)
#define FIFO_SIZE_SAMPLES (FIFO_SIZE * FLEX_SAMPLE_RATE / 1000)

#endif // FLEX_DEFINES_H
