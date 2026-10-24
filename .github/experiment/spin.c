// EXPERIMENT (to be reverted): keep a CPU from going idle, either by spinning
// ("spin") or by waking every millisecond ("tick").
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    if (argc > 1 && !strcmp(argv[1], "tick"))
        for (;;) usleep(1000);
    for (volatile unsigned long n = 0;; n++) {}
}
