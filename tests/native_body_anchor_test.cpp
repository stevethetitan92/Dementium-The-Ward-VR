#include "../src/native_body_anchor.hpp"
#include <cstdio>
#include <limits>
#include <stdexcept>
static void check(bool v){if(!v)throw std::runtime_error("native body anchor failed");}
int main(int argc,char** argv){try{
    check(argc==2);FILE* f=nullptr;fopen_s(&f,argv[1],"rb");check(f!=nullptr);
    unsigned count=0;check(fread(&count,sizeof count,1,f)==1&&count>0);float worst=0;
    for(unsigned i=0;i<count;i++){
        float row[38];check(fread(row,sizeof row,1,f)==1);float actual[12];std::memcpy(actual,row+14,sizeof actual);
        check(nativeBodyAnchor::stabilize(row,row+7,actual));
        for(unsigned k=0;k<12;k++){const float e=std::fabs(actual[k]-row[26+k]);if(e>worst)worst=e;check(e<.00001f);}
    }check(fgetc(f)==EOF);fclose(f);
    float reference[]={0,0,0,1,0,0,0},invalid[]={0,0,0,0,0,0,0},entity[]={1,0,0,2,0,1,0,3,0,0,1,4},saved[12];
    std::memcpy(saved,entity,sizeof saved);check(!nativeBodyAnchor::stabilize(reference,invalid,entity));check(!std::memcmp(entity,saved,sizeof saved));
    invalid[3]=std::numeric_limits<float>::quiet_NaN();check(!nativeBodyAnchor::stabilize(reference,invalid,entity));check(!std::memcmp(entity,saved,sizeof saved));
    std::printf("PASS: %u native clip samples match independent affine compensation; max error %.9g; invalid input leaves entity unchanged.\n",count,worst);return 0;
}catch(const std::exception& e){std::puts(e.what());return 1;}}
