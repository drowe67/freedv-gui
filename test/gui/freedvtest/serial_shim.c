/*
 * Gives pseudo-terminals the control lines of a real serial port, which is
 * what FreeDV uses to key a radio (RTS/DTR) and to be told to transmit (CTS).
 * Pseudo-terminals have no such lines, so their state is kept in files:
 *
 *   $FREEDVTEST_SERIAL_DIR/<n>.lines    RTS and DTR as last set, e.g. "RTS=1 DTR=0"
 *   $FREEDVTEST_SERIAL_DIR/<n>.history  every state they've been in, one per line
 *   $FREEDVTEST_SERIAL_DIR/<n>.cts      "1" while CTS is to read as raised
 *
 * where <n> is from the port's name (/dev/pts/<n>, or /dev/ttys<n> on macOS).
 * Loaded into FreeDV with LD_PRELOAD (DYLD_INSERT_LIBRARIES on macOS) by
 * freedvtest.serial; anything that isn't a terminal, or isn't about control
 * lines, is passed on untouched.
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define MAX_PORTS 1024

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int lines[MAX_PORTS];

/* Returns <n> for a pseudo-terminal, or -1 for anything else. */
static int port_number(int fd)
{
    char name[PATH_MAX];
    int number = -1;

    if (ttyname_r(fd, name, sizeof(name)) != 0) return -1;
    if (sscanf(name, "/dev/pts/%d", &number) != 1 && sscanf(name, "/dev/ttys%d", &number) != 1) return -1;
    return (number >= 0 && number < MAX_PORTS) ? number : -1;
}

static void record(const char* directory, int port)
{
    char path[PATH_MAX];
    char state[32];
    FILE* file;

    snprintf(state, sizeof(state), "RTS=%d DTR=%d\n", (lines[port] & TIOCM_RTS) != 0, (lines[port] & TIOCM_DTR) != 0);

    snprintf(path, sizeof(path), "%s/%d.lines", directory, port);
    file = fopen(path, "w");
    if (file != NULL)
    {
        fputs(state, file);
        fclose(file);
    }

    snprintf(path, sizeof(path), "%s/%d.history", directory, port);
    file = fopen(path, "a");
    if (file != NULL)
    {
        fputs(state, file);
        fclose(file);
    }
}

static int cts(const char* directory, int port)
{
    char path[PATH_MAX];
    int raised = 0;
    FILE* file;

    snprintf(path, sizeof(path), "%s/%d.cts", directory, port);
    file = fopen(path, "r");
    if (file != NULL)
    {
        raised = (fgetc(file) == '1');
        fclose(file);
    }
    return raised;
}

#if defined(__APPLE__)
/* macOS only uses a function in place of another if it's said to be in
 * place of it (below), and inside this library ioctl() is still the real
 * one. */
static int shim_ioctl(int fd, unsigned long request, ...)
#else
int ioctl(int fd, unsigned long request, ...)
#endif
{
#if defined(__APPLE__)
    int (*real_ioctl)(int, unsigned long, ...) = ioctl;
#else
    static int (*real_ioctl)(int, unsigned long, ...) = NULL;
#endif
    const char* directory = getenv("FREEDVTEST_SERIAL_DIR");
    va_list args;
    void* argument;
    int port;

    va_start(args, request);
    argument = va_arg(args, void*);
    va_end(args);

    if (real_ioctl == NULL)
    {
        real_ioctl = (int (*)(int, unsigned long, ...))dlsym(RTLD_NEXT, "ioctl");
    }

    if (directory == NULL || argument == NULL ||
        (request != TIOCMBIS && request != TIOCMBIC && request != TIOCMSET && request != TIOCMGET) ||
        (port = port_number(fd)) < 0)
    {
        return real_ioctl(fd, request, argument);
    }

    pthread_mutex_lock(&lock);
    if (request == TIOCMGET)
    {
        *(int*)argument = (lines[port] & (TIOCM_RTS | TIOCM_DTR)) | (cts(directory, port) ? TIOCM_CTS : 0);
    }
    else
    {
        int bits = *(int*)argument & (TIOCM_RTS | TIOCM_DTR);
        if (request == TIOCMBIS) lines[port] |= bits;
        else if (request == TIOCMBIC) lines[port] &= ~bits;
        else lines[port] = bits;
        record(directory, port);
    }
    pthread_mutex_unlock(&lock);
    return 0;
}

#if defined(__APPLE__)
__attribute__((used)) static struct
{
    const void* replacement;
    const void* replaced;
} interpose[] __attribute__((section("__DATA,__interpose"))) = {
    { (const void*)shim_ioctl, (const void*)ioctl },
};
#endif
