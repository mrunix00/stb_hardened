#ifdef __cplusplus
extern "C" {
#endif

#define STB_IMAGE_IMPLEMENTATION

#include "stb_image.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * Comprehensive fuzzer for stb_image.h.
 *
 * Layout: first byte selects which API to exercise; remaining bytes
 * are the image payload. We use req_comp values 0..4 to exercise
 * the format-conversion paths that have historically been a major
 * source of bugs (see BUG-006, BUG-011, BUG-014, BUG-019).
 */

static void reset_flip(void) {
    stbi_set_flip_vertically_on_load(0);
    stbi_set_flip_vertically_on_load_thread(0);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 2) return 0;

    reset_flip();

    uint8_t mode = data[0];
    const uint8_t* payload = data + 1;
    size_t payload_size = size - 1;

    int x = 0, y = 0, channels = 0;
    int reqs[5] = { 0, 1, 2, 3, 4 };
    int req = reqs[mode % 5];

    /* occasional vertical-flip pass — has its own bug history */
    int flip = (mode & 0x80) ? 1 : 0;
    stbi_set_flip_vertically_on_load(flip);

    switch (mode & 0x7) {
    case 0: {
        /* 8-bit load, varied req_comp */
        unsigned char *img = stbi_load_from_memory(payload, (int)payload_size,
                                                   &x, &y, &channels, req);
        if (img) stbi_image_free(img);
        break;
    }
    case 1: {
        /* 16-bit load, varied req_comp */
        stbi_us *img = stbi_load_16_from_memory(payload, (int)payload_size,
                                                &x, &y, &channels, req);
        if (img) stbi_image_free(img);
        break;
    }
    case 2: {
        /* float/HDR load, varied req_comp */
        float *img = stbi_loadf_from_memory(payload, (int)payload_size,
                                            &x, &y, &channels, req);
        if (img) stbi_image_free(img);
        break;
    }
    case 3: {
        /* GIF multi-frame load, varied req_comp */
        int *delays = NULL;
        int gx = 0, gy = 0, gz = 0, gcomp = 0;
        unsigned char *img = stbi_load_gif_from_memory(payload, (int)payload_size,
                                                       &delays, &gx, &gy, &gz, &gcomp, req);
        if (img) stbi_image_free(img);
        if (delays) free(delays);
        break;
    }
    case 4: {
        /* info-only path */
        stbi_info_from_memory(payload, (int)payload_size, &x, &y, &channels);
        break;
    }
    case 5: {
        /* 8-bit with alternative req (skip the 0 default) */
        unsigned char *img = stbi_load_from_memory(payload, (int)payload_size,
                                                   &x, &y, &channels, req ? req : 1);
        if (img) stbi_image_free(img);
        break;
    }
    case 6: {
        /* 16-bit with alternative req (skip the 0 default) */
        stbi_us *img = stbi_load_16_from_memory(payload, (int)payload_size,
                                                &x, &y, &channels, req ? req : 1);
        if (img) stbi_image_free(img);
        break;
    }
    case 7: {
        /* HDR with alternative req (skip the 0 default) */
        float *img = stbi_loadf_from_memory(payload, (int)payload_size,
                                            &x, &y, &channels, req ? req : 1);
        if (img) stbi_image_free(img);
        break;
    }
    }

    reset_flip();
    return 0;
}

#ifdef __cplusplus
}
#endif
