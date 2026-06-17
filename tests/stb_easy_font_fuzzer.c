#ifdef __cplusplus
extern "C" {
#endif

#define STB_EASY_FONT_IMPLEMENTATION

#include "stb_easy_font.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * Fuzzer for stb_easy_font.h.
 *
 * The input data is split:
 *   byte[0]    — mode / parameters
 *   byte[1..13] — 4 floats for x, y (3 each) + buffer scaling hint
 *   remaining  — text payload (null-terminated)
 *
 * Modes (derived from byte[0] & 7):
 *   0-3: stb_easy_font_print with varying vbuf_size
 *   4:   stb_easy_font_width
 *   5:   stb_easy_font_height
 *   6-7: stb_easy_font_print with spacing variations
 */

/* scratch buffer large enough to absorb any output without OOM */
static char scratch[512 * 1024];

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* need at least 1 byte for mode */
    if (size < 1) return 0;

    uint8_t mode = data[0];
    uint8_t param_bits[14];
    size_t text_offset;

    if (size >= 15) {
        memcpy(param_bits, data + 1, 13);
        text_offset = 14;
    } else {
        memset(param_bits, 0, sizeof(param_bits));
        text_offset = size;
    }

    /* ensure null-terminated text — copy what we can, cap at scratch size */
    size_t text_len = size - text_offset;
    if (text_len > sizeof(scratch) - 1)
        text_len = sizeof(scratch) - 1;
    memcpy(scratch, data + text_offset, text_len);
    scratch[text_len] = '\0';

    /* derive float params from the param bits */
    float x = (float)(int)param_bits[0] + (float)param_bits[1] / 256.0f;
    float y = (float)(int)param_bits[2] + (float)param_bits[3] / 256.0f;

    /* vbuf_size: scale from param bits, or use defaults per mode */
    int vbuf_size = (int)((unsigned int)param_bits[4] * 256u + (unsigned int)param_bits[5]);
    if (vbuf_size < 0) vbuf_size = 0;
    if (vbuf_size > (int)sizeof(scratch)) vbuf_size = (int)sizeof(scratch);

    unsigned char color[4];
    color[0] = param_bits[6];
    color[1] = param_bits[7];
    color[2] = param_bits[8];
    color[3] = param_bits[9];

    /* spacing from param bits (may be negative) */
    float spacing = (float)(int)param_bits[10] + (float)param_bits[11] / 256.0f;

    int result;

    switch (mode & 7) {
    case 0:
        /* print with color and buffer */
        if (vbuf_size >= 64) {
            stb_easy_font_spacing(spacing);
            result = stb_easy_font_print(x, y, scratch, color, scratch + vbuf_size / 2, vbuf_size / 2);
            (void)result;
        }
        break;

    case 1:
        /* print with NULL color (defaults to white) */
        if (vbuf_size >= 64) {
            stb_easy_font_spacing(spacing * 0.5f);
            result = stb_easy_font_print(x, y, scratch, NULL, scratch, vbuf_size);
            (void)result;
        }
        break;

    case 2:
        /* print with tiny buffer — forces truncation path */
        {
            int tiny = (vbuf_size % 200) + 1;
            result = stb_easy_font_print(x, y, scratch, NULL, scratch, tiny);
            (void)result;
        }
        break;

    case 3:
        /* print with zero-size or minimal buffer */
        result = stb_easy_font_print(x, y, scratch, NULL, scratch, vbuf_size % 64);
        (void)result;
        break;

    case 4:
        /* width only */
        stb_easy_font_spacing(spacing);
        result = stb_easy_font_width(scratch);
        (void)result;
        break;

    case 5:
        /* height only */
        result = stb_easy_font_height(scratch);
        (void)result;
        break;

    case 6:
        /* print with strongly negative spacing */
        {
            float neg_spacing = spacing - 2.0f;
            if (neg_spacing < -10.0f) neg_spacing = -10.0f;
            stb_easy_font_spacing(neg_spacing);
            if (vbuf_size >= 64) {
                result = stb_easy_font_print(x, y, scratch, color, scratch, vbuf_size);
                (void)result;
            }
        }
        break;

    case 7:
        /* print with very long text repeated — stress test */
        {
            /* use the text as-is; may be long */
            stb_easy_font_spacing(spacing * 2.0f);
            if (vbuf_size >= 64) {
                result = stb_easy_font_print(x, y, scratch, color, scratch, vbuf_size);
                (void)result;
            }
        }
        break;
    }

    /* reset spacing to avoid carry-over */
    stb_easy_font_spacing(0);

    return 0;
}

#ifdef __cplusplus
}
#endif
