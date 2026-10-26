/* Host-only cross-platform 8x8 to 16x16 converter. */
#include "tunuino/calibration.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tunuino_map source, converted, decoded;
int main(void) {
    source.kind=TUNUINO_MAP_VE;
    source.rpm_count=source.load_count=8;
    for(unsigned i=0;i<8;++i) {
        source.rpm[i]=500+(int32_t)i*700;
        source.load_milli_kpa[i]=20000+(int32_t)i*17000;
        for(unsigned j=0;j<8;++j)
            source.cells[i][j]=50000+(int32_t)i*5000+(int32_t)j*600;
    }
    assert(tunuino_map_refine_16(&source,&converted)==TUNUINO_MAP_OK);
    assert(converted.rpm_count==16 && converted.load_count==16);
    for(unsigned i=0;i<8;++i) {
        int row=-1,col=-1;
        for(unsigned j=0;j<16;++j) {
            if(converted.rpm[j]==source.rpm[i])col=(int)j;
            if(converted.load_milli_kpa[j]==source.load_milli_kpa[i])row=(int)j;
        }
        assert(row>=0&&col>=0);
        for(unsigned j=0;j<8;++j) {
            int col2=-1;
            for(unsigned k=0;k<16;++k)
                if(converted.rpm[k]==source.rpm[j])col2=(int)k;
            assert(col2>=0&&converted.cells[row][col2]==source.cells[i][j]);
        }
    }
    assert(tunuino_map_validate(&converted)==TUNUINO_MAP_OK);
    uint8_t serialized[TUNUINO_MAP_FILE_MAX];size_t count=0;
    assert(tunuino_map_encode(&converted,serialized,sizeof(serialized),&count)==TUNUINO_MAP_OK);
    assert(count==1168);
    assert(tunuino_map_decode(serialized,count,&decoded)==TUNUINO_MAP_OK);
    assert(memcmp(decoded.rpm,converted.rpm,16*sizeof(int32_t))==0);
    printf("refined_ve ");
    for(size_t i=0;i<count;++i)printf("%02x",serialized[i]);
    puts("");
    tunuino_map before=converted;
    for(unsigned i=0;i<8;++i) {source.rpm[i]=(int32_t)i;source.load_milli_kpa[i]=(int32_t)i;}
    assert(tunuino_map_refine_16(&source,&converted)==TUNUINO_MAP_RANGE);
    assert(memcmp(&before,&converted,sizeof(before))==0);
    assert(tunuino_map_refine_16(&source,&source)==TUNUINO_MAP_ARGUMENT);
    puts("C 8x8 to 16x16 calibration refinement, original knots and immutable failure passed");
    return 0;
}
