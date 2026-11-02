#include "../src/upload_capture.hpp"
#include <cassert>
#include <cstdio>
#include <random>
#include <limits>
int main(){
    UploadCapture capture;unsigned char live[4]={1,2,3,4},copy[2]={8,9};
    capture.mapped(live,4);capture.unmapped(0,4);
    assert(capture.cpu!=live&&capture.cpu[0]==1);
    capture.mapped(live,4);live[0]=7;
    capture.copied(4,0,copy,2);
    assert(capture.cpu==live&&capture.cpu[0]==7); // prior shadow must not take over
    capture.mapped(live,4);capture.unmapped(0,0);
    assert(capture.cpu==live); // nested Map requires matching final Unmap
    capture.unmapped(0,1);
    assert(capture.cpu!=live&&capture.cpu[0]==7&&capture.cpu[1]==2);
    capture.copied(4,2,copy,2);
    assert(capture.cpu[0]==7&&capture.cpu[2]==8&&capture.cpu[3]==9);
    capture.copied(4,3,copy,2);assert(capture.cpu[3]==9);
    capture.unmapped(0,4);assert(capture.cpu[0]==7);
    capture.mapped(live,4);capture.mapped(nullptr,4);live[1]=6;
    assert(capture.mappings==2&&capture.cpu==live);
    capture.unmapped(0,0);assert(capture.mappings==1&&capture.cpu==live);
    capture.unmapped(1,2);assert(capture.mappings==0&&capture.cpu[1]==6);
    UploadCapture pointerless;pointerless.mapped(nullptr,4);
    assert(pointerless.mappings==1&&!pointerless.cpu);
    pointerless.mapped(live,4);pointerless.unmapped(0,0);
    assert(pointerless.mappings==1&&pointerless.cpu==live);
    pointerless.unmapped(0,4);assert(pointerless.cpu[1]==6);
    UploadCapture partial;
    partial.copied(16,4,copy,2);
    assert(partial.canRead(4,2)&&!partial.canRead(0,1)&&!partial.canRead(6,1));
    assert(!partial.canRead(3,3)&&!partial.canRead(4,3)&&!partial.canRead(15,2));
    partial.copied(16,8,copy,2);assert(!partial.canRead(4,6));
    partial.copied(16,6,copy,2);assert(partial.canRead(4,6)); // adjacent regions join
    partial.copied(16,3,copy,2);assert(partial.canRead(3,7)&&!partial.canRead(2,8));
    partial.copied(8,0,copy,2);assert(partial.canRead(0,2)&&!partial.canRead(4,2));
    UploadCapture partialUnmap;partialUnmap.mapped(live,4);
    assert(partialUnmap.canRead(0,4));partialUnmap.unmapped(1,3);
    assert(partialUnmap.canRead(1,2)&&!partialUnmap.canRead(0,1)&&!partialUnmap.canRead(3,1));
    partialUnmap.mapped(live,4);partialUnmap.unmapped(3,4);
    assert(partialUnmap.canRead(1,3)&&!partialUnmap.canRead(0,4));
    assert(!partialUnmap.canRead((std::numeric_limits<size_t>::max)(),2));
    UploadCapture coverage;unsigned char source[64]{};bool oracle[64]{};std::mt19937 random(10232026);
    coverage.copied(64,0,source,0);
    for(unsigned step=0;step<512;++step){
        size_t offset=random()%65,count=random()%65;
        coverage.copied(64,offset,source,count);
        if(count<=64-offset)for(size_t j=offset;j<offset+count;++j)oracle[j]=true;
        for(unsigned probe=0;probe<16;++probe){
            offset=random()%65;count=random()%65;bool expected=count<=64-offset;
            if(expected)for(size_t j=offset;j<offset+count;++j)expected=expected&&oracle[j];
            assert(coverage.canRead(offset,count)==expected);
        }
    }
    std::puts("PASS: upload remap, pointerless/nested mapping, partial copies, 8192 byte-coverage oracle checks and bounds; no game/headset.");
}
