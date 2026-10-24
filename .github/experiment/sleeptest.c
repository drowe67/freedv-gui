// EXPERIMENT (to be reverted): how long a 20 ms sleep takes here, on the
// main thread and other threads (on macOS, at each QoS class).
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <dispatch/dispatch.h>
#endif

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }

static void measure(const char* label)
{
    double worst = 0, sum = 0, best = 1;
    for (int i = 0; i < 50; i++) {
        double t = now(); usleep(20000); double d = now() - t;
        sum += d; if (d > worst) worst = d; if (d < best) best = d;
    }
    printf("SLEEPTEST %-32s 20 ms sleep: best %6.1f mean %6.1f worst %6.1f ms\n", label, best * 1e3, sum / 50 * 1e3, worst * 1e3);
}

static void* thread_main(void* arg) { measure((const char*)arg); return NULL; }

int main(void)
{
    measure("main thread");
    pthread_t t;
    pthread_create(&t, NULL, thread_main, "new thread");
    pthread_join(t, NULL);
#if defined(__APPLE__)
    struct { qos_class_t q; const char* n; } qs[] = {
        { QOS_CLASS_USER_INTERACTIVE, "queue USER_INTERACTIVE" }, { QOS_CLASS_USER_INITIATED, "queue USER_INITIATED" },
        { QOS_CLASS_DEFAULT, "queue DEFAULT" }, { QOS_CLASS_UTILITY, "queue UTILITY" }, { QOS_CLASS_BACKGROUND, "queue BACKGROUND" } };
    for (int i = 0; i < 5; i++) {
        dispatch_queue_t q = dispatch_queue_create("t", dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, qs[i].q, 0));
        const char* n = qs[i].n;
        dispatch_semaphore_t done = dispatch_semaphore_create(0);
        dispatch_async(q, ^{ measure(n); dispatch_semaphore_signal(done); });
        dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
    }
#endif
    return 0;
}
