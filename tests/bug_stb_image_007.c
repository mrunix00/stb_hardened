#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    int failures = 0;
    printf("BUG-stb_image-007: Testing unchecked stbi__getn in TGA/HDR...\n");

    // Test 1: truncated TGA file (valid header, no pixel data)
    {
        unsigned char tga[] = {
            0,          // ID length
            0,          // color map type
            2,          // image type = uncompressed true-color
            0,0,0,0,0,  // color map spec (5 bytes)
            0,0,        // x origin
            0,0,        // y origin
            32,0,       // width = 32
            32,0,       // height = 32
            24,         // bits per pixel
            0           // descriptor
        };
        int x, y, comp;
        stbi_uc *img = stbi_load_from_memory(tga, sizeof(tga), &x, &y, &comp, 4);
        // Prior to the fix, this SUCCEEDS (returns uninitialized heap data)
        // After the fix, it should FAIL (truncated file detected)
        if (img != NULL) {
            printf("  TGA truncated: loaded %d bytes (BUG CONFIRMED: uninit data leaked)\n",
                   x * y * 4);
            stbi_image_free(img);
            failures++;
        } else {
            printf("  TGA truncated: FAILED as expected (%s)\n", stbi_failure_reason());
        }
    }

    // Test 2: truncated HDR file (valid header, no pixel data)
    {
        unsigned char hdr[] = {
            '#','?','R','A','D','I','A','N','C','E','\n',
            'F','O','R','M','A','T','=','3','2','-','b','i','t','_','r','l','e','_','r','g','b','e','\n',
            '\n',
            '-','Y',' ','3','2',' ','+','X',' ','3','2','\n'
        };
        int x, y, comp;
        float *img = stbi_loadf_from_memory(hdr, sizeof(hdr), &x, &y, &comp, 4);
        // Prior to the fix, this SUCCEEDS (returns uninitialized stack data)
        // After the fix, it should FAIL (truncated file detected)
        if (img != NULL) {
            printf("  HDR truncated: loaded (BUG CONFIRMED: uninit data leaked)\n");
            stbi_image_free(img);
            failures++;
        } else {
            printf("  HDR truncated: FAILED as expected (%s)\n", stbi_failure_reason());
        }
    }

    if (failures) {
        printf("BUG CONFIRMED: %d truncated files loaded successfully (information disclosure)\n", failures);
        return 1;
    }
    printf("ALL PASSED\n");
    return 0;
}
