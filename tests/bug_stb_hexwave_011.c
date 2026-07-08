#define STB_HEXWAVE_IMPLEMENTATION
#include "stb_hexwave.h"
#include <stdio.h>
#include <stdlib.h>

/* BUG-STB-HEXWAVE-011: memory leak on re-initialization.
 *
 * hexwave_init() (NULL / library-allocated path) heap-allocates blep/blamp
 * but never frees a previous allocation when called more than once without
 * an intervening hexwave_shutdown(). The first allocation is leaked.
 *
 * Expected (bug present, unpatched): LeakSanitizer reports the first
 * blep/blamp allocations as leaked -> non-zero exit.
 * Expected (fixed): re-init frees the previous library-owned tables -> exit 0.
 */
int main(void)
{
    hexwave_init(32, 16, NULL);
    hexwave_init(32, 16, NULL);   /* first blep/blamp allocation is leaked */
    hexwave_shutdown(NULL);       /* only frees the second allocation */
    printf("BUG-STB-HEXWAVE-011: re-init completed\n");
    return 0;
}
