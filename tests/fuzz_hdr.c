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
    int x = 0, y = 0, channels = 0;
    int reqs[5] = { 0, 1, 2, 3, 4 };
    int req = reqs[data[0] % 5];

    float *img = stbi_loadf_from_memory(data, (int)size, &x, &y, &channels, req);
    if (img) stbi_image_free(img);
    return 0;
}

#ifdef __cplusplus
}
#endif
