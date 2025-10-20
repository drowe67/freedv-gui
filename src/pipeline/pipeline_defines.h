#ifndef PIPELINE_DEFINES_H
#define PIPELINE_DEFINES_H

#ifndef FRAME_DURATION_MS
#define FRAME_DURATION_MS   20                             // default frame length of 20 mS = 0.02 seconds
#endif // !FRAME_DURATION_MS

#ifndef MS_TO_SEC
#define MS_TO_SEC           1000
#endif // !MS_TO_SEC

// Real-time memory block size
#ifndef CODEC2_REAL_TIME_MEMORY_SIZE
#define CODEC2_REAL_TIME_MEMORY_SIZE (512*1024)
#endif // !CODEC2_REAL_TIME_MEMORY_SIZE

#ifndef FIFO_SIZE
#define FIFO_SIZE           640                            // default fifo size in ms
#endif // !FIFO_SIZE

#endif // PIPELINE_DEFINES_H