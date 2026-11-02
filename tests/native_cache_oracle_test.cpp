#include "../src/native_transform_cache.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <random>
#include <stdexcept>
using Affine=std::array<float,12>;
// Independent column-vector 3x4 composition, without DirectX matrix helpers.
static Affine compose(const Affine& a,const Affine& b){
    Affine result{};
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c){
        double sum=c==3?a[r*4+3]:0;
        for(unsigned k=0;k<3;++k)sum+=double(a[r*4+k])*b[k*4+c];
        result[r*4+c]=static_cast<float>(sum);
    }
    return result;
}
static Affine randomAffine(std::mt19937& random){
    std::uniform_real_distribution<float> angle(-3.14f,3.14f),position(-20,20),scale(.4f,2.5f);
    float x=angle(random),y=angle(random),z=angle(random);
    Affine rx{1,0,0,0,0,cosf(x),-sinf(x),0,0,sinf(x),cosf(x),0};
    Affine ry{cosf(y),0,sinf(y),0,0,1,0,0,-sinf(y),0,cosf(y),0};
    Affine rz{cosf(z),-sinf(z),0,0,sinf(z),cosf(z),0,0,0,0,1,0};
    auto a=compose(rz,compose(ry,rx));
    for(unsigned c=0;c<3;++c){float s=scale(random);for(unsigned r=0;r<3;++r)a[r*4+c]*=s;}
    a[3]=position(random);a[7]=position(random);a[11]=position(random);return a;
}
static void compare(const Affine& actual,const Affine& expected){
    for(unsigned i=0;i<12;++i)if(!std::isfinite(actual[i])||fabsf(actual[i]-expected[i])>.00015f*(1+fabsf(expected[i])))
        throw std::runtime_error("native cached entity replacement changed the parent camera transform");
}
int main(){try{
    std::mt19937 random(0x23102026);
    for(unsigned i=0;i<10000;++i){
        auto parent=randomAffine(random),oldEntity=randomAffine(random),newEntity=randomAffine(random);
        auto cached=compose(parent,oldEntity),expected=compose(parent,newEntity);Affine output{};
        if(!nativeTransformCache::replace(cached.data(),oldEntity.data(),newEntity.data(),output.data()))throw std::runtime_error("valid cached transform rejected");
        compare(output,expected);
        // Production supports an aliased output when restoring a cached slot.
        if(!nativeTransformCache::replace(output.data(),newEntity.data(),oldEntity.data(),output.data()))throw std::runtime_error("cache restoration rejected");
        compare(output,cached);
    }
    std::puts("PASS: 10000 independent parent/entity composition and round-trip checks with rotations, translation and nonuniform scale; no game/headset.");return 0;
}catch(const std::exception& e){std::puts(e.what());return 1;}}
