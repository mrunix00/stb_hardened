#ifdef __cplusplus
extern "C" {
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    stbi_set_flip_vertically_on_load(0);
    if (size < 1) return 0;
    int *delays = NULL;
    int gx = 0, gy = 0, gz = 0, gcomp = 0;
    int reqs[5] = { 0, 1, 2, 3, 4 };
    int req = reqs[data[0] % 5];
    data++; size--;
    if (size < 1) return 0;

    unsigned char *img = stbi_load_gif_from_memory(data, (int)size, &delays, &gx, &gy, &gz, &gcomp, req);
    if (img) stbi_image_free(img);
    if (delays) free(delays);
    return 0;
}

#ifdef __cplusplus
}
#endif
