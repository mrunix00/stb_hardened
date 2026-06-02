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

    if (size < 2) return 0;

    uint8_t mode = data[0];
    const uint8_t* payload = data + 1;
    size_t payload_size = size - 1;

    int x = 0, y = 0, channels = 0;
    int reqs[5] = { 0, 1, 2, 3, 4 };
    int req = reqs[mode % 5];

    int flip = (mode & 0x80) ? 1 : 0;
    stbi_set_flip_vertically_on_load(flip);

    int *delays = NULL;
    int gx = 0, gy = 0, gz = 0, gcomp = 0;
    unsigned char *img = stbi_load_gif_from_memory(payload, (int)payload_size,
                                                   &delays, &gx, &gy, &gz, &gcomp, req);
    if (img) stbi_image_free(img);
    if (delays) free(delays);

    stbi_set_flip_vertically_on_load(0);
    return 0;
}

#ifdef __cplusplus
}
#endif
