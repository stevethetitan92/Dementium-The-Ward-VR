#pragma once
#include <cstdio>
#include <vector>
#include <cstddef>
namespace ppmCapture {
// Same RGB P6 pixels as the old per-pixel writer, including padded GPU rows.
// One buffered write per row avoids millions of stdio calls on the render thread.
inline bool write(FILE* file,const unsigned char* rgba,std::size_t pitch,unsigned imageWidth,unsigned imageHeight,unsigned step=1){
 if(!file||!rgba||!step)return false;unsigned width=imageWidth/step,height=imageHeight/step;
 if(!width||!height||std::size_t(width)>std::size_t(-1)/3)return false;
 if(fprintf(file,"P6\n%u %u\n255\n",width,height)<0)return false;
 std::vector<unsigned char> row(std::size_t(width)*3);
 for(unsigned y=0;y<height;++y){auto source=rgba+std::size_t(y)*step*pitch;for(unsigned x=0;x<width;++x){auto pixel=source+std::size_t(x)*step*4;row[x*3]=pixel[0];row[x*3+1]=pixel[1];row[x*3+2]=pixel[2];}if(fwrite(row.data(),1,row.size(),file)!=row.size())return false;}return true;
}
}
