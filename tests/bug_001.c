#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../stb_truetype.h"

static unsigned char* load_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    unsigned char *buf = (unsigned char*)malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(buf); return NULL; }
    fclose(f);
    *out_size = (size_t)sz;
    return buf;
}

int main(int argc, char **argv) {
    const char *path = (argc > 1) ? argv[1] : "tests/bug_001_hbf1.ttf";
    size_t sz = 0;
    unsigned char *buf = load_file(path, &sz);
    if (!buf) {
        fprintf(stderr, "Failed to read %s\n", path);
        return 2;
    }

    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, buf, 0)) {
        fprintf(stderr, "stbtt_InitFont failed\n");
        free(buf);
        return 3;
    }

    int x0=0,y0=0,x1=0,y1=0;
    stbtt_GetCodepointBox(&font, 'A', &x0, &y0, &x1, &y1);

    stbtt_vertex *verts = NULL;
    int num = stbtt_GetGlyphShape(&font, stbtt_FindGlyphIndex(&font, 'A'), &verts);
    if (num > 0 && verts) {
        STBTT_free(verts, font.userdata);
    }

    free(buf);
    return 0;
}
