// EXPERIMENT (to be reverted): totals up how long each sleep was asked for
// and how long it took, by caller and by length asked for. Preload it
// (DYLD_INSERT_LIBRARIES / LD_PRELOAD); it reports when the program exits.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <execinfo.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAXS 256
struct stat_t { const char* name; long n; double asked, took; };
static struct stat_t callers[MAXS], lengths[MAXS];
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static __thread int inside;

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }

static void add(struct stat_t* s, const char* name, double asked, double took)
{
    int i = 0;
    for (; i < MAXS && s[i].name; i++) if (!strcmp(s[i].name, name)) break;
    if (i == MAXS) return;
    if (!s[i].name) s[i].name = strdup(name);
    s[i].n++; s[i].asked += asked; s[i].took += took;
}

static void note(double asked, double took)
{
    void* bt[8]; int n = backtrace(bt, 8);
    const char* who = "?";
    char buf[64];
    for (int i = 2; i < n; i++) {
        Dl_info info;
        if (!dladdr(bt[i], &info)) continue;
        if (info.dli_sname && (strstr(info.dli_sname, "sleep") || strstr(info.dli_sname, "note"))) continue;
        who = info.dli_sname ? info.dli_sname : (info.dli_fname ? strrchr(info.dli_fname, '/') + 1 : "?");
        break;
    }
    double ms = asked * 1e3;
    snprintf(buf, sizeof buf, "%s", ms < 1 ? "<1 ms" : ms < 5 ? "1-5 ms" : ms < 15 ? "5-15 ms" : ms < 25 ? "15-25 ms" : ms < 75 ? "25-75 ms" : ms < 150 ? "75-150 ms" : ms < 600 ? "150-600 ms" : ">=600 ms");
    pthread_mutex_lock(&mu);
    add(callers, who, asked, took);
    add(lengths, buf, asked, took);
    pthread_mutex_unlock(&mu);
}

#if defined(__APPLE__)
#define DYLD_INTERPOSE(r, f) __attribute__((used)) static struct { const void* a; const void* b; } _i_##f \
    __attribute__((section("__DATA,__interpose"))) = { (const void*)(unsigned long)&r, (const void*)(unsigned long)&f };
static int my_usleep(useconds_t us) { double t = now(); int r = usleep(us); note(us / 1e6, now() - t); return r; }
static int my_nanosleep(const struct timespec* a, struct timespec* b) { double t = now(); int r = nanosleep(a, b); note(a->tv_sec + a->tv_nsec / 1e9, now() - t); return r; }
DYLD_INTERPOSE(my_usleep, usleep)
DYLD_INTERPOSE(my_nanosleep, nanosleep)
#else
int usleep(useconds_t us)
{
    static int (*real)(useconds_t);
    if (!real) real = (int (*)(useconds_t))dlsym(RTLD_NEXT, "usleep");
    if (inside) return real(us);
    inside = 1; double t = now(); int r = real(us); note(us / 1e6, now() - t); inside = 0; return r;
}
int nanosleep(const struct timespec* a, struct timespec* b)
{
    static int (*real)(const struct timespec*, struct timespec*);
    if (!real) real = (int (*)(const struct timespec*, struct timespec*))dlsym(RTLD_NEXT, "nanosleep");
    if (inside) return real(a, b);
    inside = 1; double t = now(); int r = real(a, b); note(a->tv_sec + a->tv_nsec / 1e9, now() - t); inside = 0; return r;
}
int clock_nanosleep(clockid_t c, int f, const struct timespec* a, struct timespec* b)
{
    static int (*real)(clockid_t, int, const struct timespec*, struct timespec*);
    if (!real) real = (int (*)(clockid_t, int, const struct timespec*, struct timespec*))dlsym(RTLD_NEXT, "clock_nanosleep");
    if (inside || (f & TIMER_ABSTIME)) return real(c, f, a, b);
    inside = 1; double t = now(); int r = real(c, f, a, b); note(a->tv_sec + a->tv_nsec / 1e9, now() - t); inside = 0; return r;
}
#endif

static void dump(const char* what, struct stat_t* s)
{
    double ta = 0, tt = 0;
    for (int i = 0; i < MAXS && s[i].name; i++) {
        fprintf(stderr, "SLEEP %-8s %-36s n=%6ld asked %8.2f s took %8.2f s (x%.2f, %.1f ms extra each)\n", what, s[i].name, s[i].n,
                s[i].asked, s[i].took, s[i].asked > 0 ? s[i].took / s[i].asked : 0, (s[i].took - s[i].asked) / s[i].n * 1e3);
        ta += s[i].asked; tt += s[i].took;
    }
    fprintf(stderr, "SLEEP %-8s total asked %.2f s took %.2f s\n", what, ta, tt);
}

__attribute__((destructor)) static void report(void) { dump("caller", callers); dump("length", lengths); }
