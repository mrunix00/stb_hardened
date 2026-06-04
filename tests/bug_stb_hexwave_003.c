#define STB_HEXWAVE_IMPLEMENTATION
#include "stb_hexwave.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include <signal.h>

static void segv_handler(int sig)
{
    (void)sig;
    fprintf(stderr, "BUG-stb_hexwave-003: caught segfault (NULL pointer deref in hexwave_init)\n");
    _exit(1);
}

int main()
{
    struct rlimit rl;
    rl.rlim_cur = 1024 * 1024 * 1024;
    rl.rlim_max = 1024 * 1024 * 1024;
    if (setrlimit(RLIMIT_DATA, &rl) != 0) {
        perror("setrlimit");
        return 1;
    }

    signal(SIGSEGV, segv_handler);
    signal(SIGBUS, segv_handler);
    signal(SIGABRT, segv_handler);

    hexwave_init(64, 33554430, NULL);
    printf("hexwave_init returned without crashing\n");
    return 0;
}
