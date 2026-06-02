#ifdef __cplusplus
extern "C" {
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <stdint.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    stbi_set_flip_vertically_on_load(0);
    if (size < 1) return 0;
    int x = 0, y = 0, channels = 0;
    int reqs[5] = { 0, 1, 2, 3, 4 };
    int req = reqs[data[0] % 5];
    const uint8_t* payload = data + 1;
    size_t payload_size = size - 1;

    stbi_us *img = stbi_load_16_from_memory(payload, (int)payload_size, &x, &y, &channels, req);
    if (img) stbi_image_free(img);
    return 0;
}

#ifdef __cplusplus
}
#endif
