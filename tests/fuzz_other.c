#ifdef __cplusplus
extern "C" {
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Tries BMP, PNM, PSD, PICT, HDR, and TGA decoders with different req_comp */
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    stbi_set_flip_vertically_on_load(0);
    if (size < 1) return 0;
    int x = 0, y = 0, channels = 0;
    int reqs[5] = { 0, 1, 2, 3, 4 };
    int req = reqs[data[0] % 5];

    /* Also try with vertical flip */
    int flip = (data[0] & 0x40) ? 1 : 0;
    stbi_set_flip_vertically_on_load(flip);

    /* try 8-bit */
    unsigned char *img = stbi_load_from_memory(data, (int)size, &x, &y, &channels, req);
    if (img) stbi_image_free(img);

    /* try 16-bit */
    stbi_us *img16 = stbi_load_16_from_memory(data, (int)size, &x, &y, &channels, req);
    if (img16) stbi_image_free(img16);

    /* try HDR */
    float *imgf = stbi_loadf_from_memory(data, (int)size, &x, &y, &channels, req);
    if (imgf) stbi_image_free(imgf);

    stbi_set_flip_vertically_on_load(0);
    return 0;
}

#ifdef __cplusplus
}
#endif
