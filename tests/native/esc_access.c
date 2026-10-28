/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define HPM_IP_FEATURE_ESC_BYTE_READ 1
#define MIN(a,b) ((a)<(b)?(a):(b))
struct hpm_esc_config { uintptr_t base; };
struct device { const void *config; };
static uint8_t ram[65536];
static size_t first, limit, cursor, expected_width, accesses;
static bool writing;
static uint32_t bus_read(uintptr_t addr, size_t width)
{
    assert(!writing && addr==cursor && addr>=first && addr+width<=limit);
    assert(width==expected_width && addr%width==0);
    cursor+=width; ++accesses;
    uint32_t word=0; memcpy(&word,ram+addr,width);return word;
}
static void bus_write(uintptr_t addr, size_t width, uint32_t word)
{
    assert(writing && addr==cursor && addr>=first && addr+width<=limit);
    assert(width==expected_width && addr%width==0);
    cursor+=width; ++accesses; memcpy(ram+addr,&word,width);
}
#include "esc_access_functions.inc"
static void Check(unsigned offset,unsigned length,unsigned alignment)
{
    uint8_t data[520], before[sizeof(ram)];
    struct hpm_esc_config cfg={0}; struct device dev={&cfg};
    expected_width=length==1?1:length==2?2:length==4?4:1;
#if defined(CONFIG_HPM_ESC_PDRAM_WORD_ACCESS)
    if(offset>=4096 && offset%4==0 && length>=8 && length%4==0) expected_width=4;
#endif
    for(size_t i=0;i<sizeof(ram);++i) ram[i]=(uint8_t)(i*17+3);
    memset(data,0xa5,sizeof(data));
    first=cursor=offset;limit=offset+length;accesses=0;writing=false;
    esc_read(&dev,offset,data+alignment,length);
    assert(cursor==limit && accesses==length/expected_width);
    assert(!memcmp(data+alignment,ram+offset,length));
    for(unsigned i=0;i<alignment;++i) assert(data[i]==0xa5);
    for(unsigned i=alignment+length;i<sizeof(data);++i) assert(data[i]==0xa5);
    for(unsigned i=0;i<length;++i) data[alignment+i]=(uint8_t)(i*31+7);
    memcpy(before,ram,sizeof(ram));
    cursor=offset;accesses=0;writing=true;
    esc_write(&dev,offset,data+alignment,length);
    assert(cursor==limit && accesses==length/expected_width);
    assert(!memcmp(ram,before,offset));
    assert(!memcmp(ram+offset,data+alignment,length));
    assert(!memcmp(ram+limit,before+limit,sizeof(ram)-limit));
}
int main(void)
{
    const unsigned offsets[]={0x220,0x810,0x1000,0x1100,0x1d00,0xfe00};
    const unsigned lengths[]={0,1,2,3,4,5,7,8,12,16,95,96,97,399,400,401,512};
    for(unsigned a=0;a<4;++a) {
        for(unsigned o=0;o<sizeof(offsets)/sizeof(offsets[0]);++o) {
            for(unsigned n=0;n<sizeof(lengths)/sizeof(lengths[0]);++n)
                Check(offsets[o],lengths[n],a);
        }
        Check(0x1101,7,a); Check(0x1102,8,a); Check(0xffff,1,a);
    }
#if defined(CONFIG_HPM_ESC_PDRAM_WORD_ACCESS)
    assert(!esc_pdram_word_access(0x10000,8));
    assert(!esc_pdram_word_access(0xfffc,8));
    assert(!esc_pdram_word_access(0x1100,SIZE_MAX));
    assert(!esc_pdram_word_access(0x800,96));
#endif
    puts("Real ESC transfer width/order/bounds/unaligned-buffer contracts PASS");
}
