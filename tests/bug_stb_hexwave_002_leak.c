#define STB_HEXWAVE_IMPLEMENTATION
#include "stb_hexwave.h"
#include <stdio.h>
#include <stdlib.h>

int main()
{
    printf("Calling hexwave_init with NULL (library-allocated buffers)...\n");
    hexwave_init(32, 16, NULL);

    printf("Calling hexwave_shutdown with NULL (should free buffers, not leak)...\n");
    hexwave_shutdown(NULL);

    printf("OK: no leak detected\n");
    return 0;
}
