// audioprobe: plays silence to one CoreAudio output device from a separate process and
// logs every I/O cycle that CoreAudio skipped or delivered late, with host times on the
// same clock as FreeDV's TIMINGDIAG lines (seconds since boot, mach_absolute_time()).
// If its cycles stall at the same moments as FreeDV's, the stalls come from coreaudiod or
// the machine's audio stack rather than from anything inside FreeDV's process.
//
//   audioprobe "<output device name>" [buffer frames, default 1024]
//
// Runs until SIGTERM/SIGINT. Build: clang -O2 -o audioprobe audioprobe.c -framework CoreAudio -framework CoreFoundation

#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_time.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define EVENTS 256

typedef struct {
    double sampleTime, expected, gapMs;
    uint64_t prevEntry, entry;
} Event;

static Event events[EVENTS];
static atomic_uint writeIndex;
static atomic_ullong cycles;
static volatile sig_atomic_t stop;
static double lastSampleTime = -1, firstSampleTime = -1;
static UInt32 lastFrames;
static uint64_t lastEntry;
static double bufferMs = 1;
static mach_timebase_info_data_t timebase;

static double hostSeconds(uint64_t t) { return (double)t * timebase.numer / timebase.denom / 1e9; }

static OSStatus ioProc(AudioObjectID dev, const AudioTimeStamp* now, const AudioBufferList* in,
                       const AudioTimeStamp* inTime, AudioBufferList* out, const AudioTimeStamp* outTime, void* ctx)
{
    (void)dev; (void)now; (void)in; (void)inTime; (void)ctx;
    uint64_t entry = mach_absolute_time();
    UInt32 frames = 0;
    for (UInt32 i = 0; i < out->mNumberBuffers; i++) {
        memset(out->mBuffers[i].mData, 0, out->mBuffers[i].mDataByteSize);
        if (i == 0 && out->mBuffers[0].mNumberChannels > 0)
            frames = out->mBuffers[0].mDataByteSize / (sizeof(Float32) * out->mBuffers[0].mNumberChannels);
    }
    if (outTime && (outTime->mFlags & kAudioTimeStampSampleTimeValid)) {
        double st = outTime->mSampleTime;
        if (firstSampleTime < 0) firstSampleTime = st;
        else {
            double expected = lastSampleTime + lastFrames;
            double gapMs = 1000.0 * (hostSeconds(entry) - hostSeconds(lastEntry));
            if (st != expected || gapMs > 1.5 * bufferMs) {
                unsigned i = atomic_load_explicit(&writeIndex, memory_order_relaxed);
                events[i % EVENTS] = (Event){ st, expected, gapMs, lastEntry, entry };
                atomic_store_explicit(&writeIndex, i + 1, memory_order_release);
            }
        }
        lastSampleTime = st;
        lastFrames = frames;
        lastEntry = entry;
    }
    atomic_fetch_add_explicit(&cycles, 1, memory_order_relaxed);
    return noErr;
}

static AudioObjectID findDevice(const char* name)
{
    AudioObjectPropertyAddress addr = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr, 0, NULL, &size) != noErr) return 0;
    AudioObjectID ids[256];
    if (size > sizeof(ids)) size = sizeof(ids);
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, NULL, &size, ids) != noErr) return 0;
    for (UInt32 i = 0; i < size / sizeof(AudioObjectID); i++) {
        AudioObjectPropertyAddress nameAddr = { kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
        CFStringRef cfName = NULL;
        UInt32 nameSize = sizeof(cfName);
        if (AudioObjectGetPropertyData(ids[i], &nameAddr, 0, NULL, &nameSize, &cfName) != noErr || !cfName) continue;
        char buf[256] = "";
        CFStringGetCString(cfName, buf, sizeof(buf), kCFStringEncodingUTF8);
        CFRelease(cfName);
        if (strcmp(buf, name) == 0) return ids[i];
    }
    return 0;
}

static void onSignal(int sig) { (void)sig; stop = 1; }

int main(int argc, char** argv)
{
    if (argc < 2) { fprintf(stderr, "usage: audioprobe \"<output device>\" [frames]\n"); return 2; }
    mach_timebase_info(&timebase);
    setvbuf(stdout, NULL, _IOLBF, 0);
    UInt32 frames = argc > 2 ? (UInt32)atoi(argv[2]) : 1024;

    AudioObjectID dev = findDevice(argv[1]);
    if (!dev) { fprintf(stderr, "AUDIOPROBE: device \"%s\" not found\n", argv[1]); return 1; }

    AudioObjectPropertyAddress bufAddr = { kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    AudioObjectSetPropertyData(dev, &bufAddr, 0, NULL, sizeof(frames), &frames);
    Float64 rate = 48000;
    AudioObjectPropertyAddress rateAddr = { kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    UInt32 rs = sizeof(rate);
    AudioObjectGetPropertyData(dev, &rateAddr, 0, NULL, &rs, &rate);
    bufferMs = 1000.0 * frames / rate;

    AudioDeviceIOProcID procId = NULL;
    if (AudioDeviceCreateIOProcID(dev, ioProc, NULL, &procId) != noErr || AudioDeviceStart(dev, procId) != noErr) {
        fprintf(stderr, "AUDIOPROBE: could not start I/O on \"%s\"\n", argv[1]);
        return 1;
    }
    printf("AUDIOPROBE \"%s\": started (device %u, %u frames at %.0f Hz, host %.3f)\n", argv[1], dev, frames, rate, hostSeconds(mach_absolute_time()));

    signal(SIGTERM, onSignal);
    signal(SIGINT, onSignal);
    unsigned readIndex = 0;
    int ticks = 0;
    while (!stop) {
        usleep(100000);
        unsigned w = atomic_load_explicit(&writeIndex, memory_order_acquire);
        if (w - readIndex > EVENTS) readIndex = w - EVENTS;
        for (; readIndex != w; readIndex++) {
            Event e = events[readIndex % EVENTS];
            printf("AUDIOPROBE \"%s\": at %.3f s: sample time jumped %+.0f frames (expected %.0f, got %.0f), %.1f ms since previous cycle (host %.3f to %.3f)\n",
                   argv[1], (e.sampleTime - firstSampleTime) / rate, e.sampleTime - e.expected, e.expected, e.sampleTime,
                   e.gapMs, hostSeconds(e.prevEntry), hostSeconds(e.entry));
        }
        if (++ticks % 50 == 0)
            printf("AUDIOPROBE \"%s\": %llu cycles so far\n", argv[1], (unsigned long long)atomic_load(&cycles));
    }
    AudioDeviceStop(dev, procId);
    AudioDeviceDestroyIOProcID(dev, procId);
    printf("AUDIOPROBE \"%s\": stopped after %llu cycles\n", argv[1], (unsigned long long)atomic_load(&cycles));
    return 0;
}
