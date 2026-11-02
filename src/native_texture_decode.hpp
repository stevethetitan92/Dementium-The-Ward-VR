#pragma once
#include <vector>
#include <array>
#include <stdexcept>
#include <cstdint>
namespace nativeTexture {
inline std::vector<unsigned char> skinPixels;
// Bounded RFC 1950/1951 reader for the locally installed texture container.
// Original implementation; no game code or proprietary pixel data is embedded.
struct Bits {
 const std::vector<unsigned char>& bytes;size_t bit=16;
 unsigned get(unsigned count){if(count>24||bit+count>bytes.size()*8)throw std::runtime_error("deflate input bounds");unsigned out=0;for(unsigned i=0;i<count;i++,bit++)out|=((bytes[bit/8]>>(bit%8))&1u)<<i;return out;}
 void align(){bit=(bit+7)&~size_t(7);}
};
struct Huffman {
 std::array<unsigned,16> count{},first{},offset{};std::vector<unsigned> symbols;
 explicit Huffman(const std::vector<unsigned>& lengths){
  for(auto n:lengths){if(n>15)throw std::runtime_error("deflate code length");if(n)count[n]++;}
  unsigned code=0,total=0;for(unsigned n=1;n<=15;n++){code=(code+count[n-1])<<1;first[n]=code;offset[n]=total;total+=count[n];if(code+count[n]>(1u<<n))throw std::runtime_error("deflate oversubscribed tree");}
  symbols.resize(total);auto next=offset;for(unsigned i=0;i<lengths.size();i++)if(lengths[i])symbols[next[lengths[i]]++]=i;
 }
 unsigned read(Bits& bits)const{unsigned code=0;for(unsigned n=1;n<=15;n++){code=(code<<1)|bits.get(1);if(code>=first[n]&&code-first[n]<count[n])return symbols[offset[n]+code-first[n]];}throw std::runtime_error("deflate invalid symbol");}
};
inline std::vector<unsigned char> inflate(const std::vector<unsigned char>& bytes,size_t maximum){
 if(bytes.size()<6||(bytes[0]&15)!=8||((unsigned(bytes[0])*256+bytes[1])%31)||bytes[1]&32)throw std::runtime_error("zlib header");
 Bits bits{bytes};std::vector<unsigned char> out;out.reserve(maximum);bool last=false;unsigned blocks=0;
 auto put=[&](unsigned n){if(out.size()>=maximum)throw std::runtime_error("deflate output bounds");out.push_back(static_cast<unsigned char>(n));};
 const unsigned lengthBase[]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
 const unsigned lengthBits[]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
 const unsigned distanceBase[]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
 const unsigned distanceBits[]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
 while(!last){if(++blocks>10000)throw std::runtime_error("deflate block limit");last=bits.get(1)!=0;unsigned type=bits.get(2);
  if(type==0){bits.align();unsigned n=bits.get(16),inverse=bits.get(16);if((n^inverse)!=65535)throw std::runtime_error("deflate stored length");for(unsigned i=0;i<n;i++)put(bits.get(8));continue;}
  if(type==3)throw std::runtime_error("deflate block type");std::vector<unsigned> literal,distance;
  if(type==1){literal.resize(288);for(unsigned i=0;i<288;i++)literal[i]=i<144?8:i<256?9:i<280?7:8;distance.assign(32,5);}
  else {
   unsigned hlit=bits.get(5)+257,hdist=bits.get(5)+1,hclen=bits.get(4)+4;if(hlit>286)throw std::runtime_error("deflate literal count");
   const unsigned canonicalOrder[]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
   std::vector<unsigned> codeLengths(19);for(unsigned i=0;i<hclen;i++)codeLengths[canonicalOrder[i]]=bits.get(3);
   Huffman tree(codeLengths);std::vector<unsigned> lengths;
   while(lengths.size()<hlit+hdist){unsigned symbol=tree.read(bits);
    if(symbol<16)lengths.push_back(symbol);else{
     unsigned repeat=0,value=0;if(symbol==16){if(lengths.empty())throw std::runtime_error("deflate repeat");repeat=bits.get(2)+3;value=lengths.back();}
     else if(symbol==17)repeat=bits.get(3)+3;else if(symbol==18)repeat=bits.get(7)+11;else throw std::runtime_error("deflate length symbol");
     if(lengths.size()+repeat>hlit+hdist)throw std::runtime_error("deflate repeat bounds");lengths.insert(lengths.end(),repeat,value);
    }
   }
   literal.assign(lengths.begin(),lengths.begin()+hlit);distance.assign(lengths.begin()+hlit,lengths.end());
  }
  if(literal.size()<=256||literal[256]==0)throw std::runtime_error("deflate missing end");Huffman lt(literal),dt(distance);
  for(;;){unsigned symbol=lt.read(bits);if(symbol<256){put(symbol);continue;}if(symbol==256)break;if(symbol>285)throw std::runtime_error("deflate length range");
   unsigned i=symbol-257,length=lengthBase[i]+bits.get(lengthBits[i]),d=dt.read(bits);if(d>=30)throw std::runtime_error("deflate distance range");unsigned back=distanceBase[d]+bits.get(distanceBits[d]);if(back>out.size())throw std::runtime_error("deflate backreference");
   for(unsigned n=0;n<length;n++)put(out[out.size()-back]);
  }
 }
 bits.align();size_t p=bits.bit/8;if(p+4>bytes.size())throw std::runtime_error("zlib checksum bounds");
 unsigned checksum=(unsigned(bytes[p])<<24)|(unsigned(bytes[p+1])<<16)|(unsigned(bytes[p+2])<<8)|bytes[p+3];unsigned a=1,b=0;for(auto x:out){a=(a+x)%65521;b=(b+a)%65521;}if(((b<<16)|a)!=checksum)throw std::runtime_error("zlib checksum");
 return out;
}
inline std::vector<unsigned char> rgba(const std::vector<unsigned char>& texture){
 if(texture.size()<40)throw std::runtime_error("texture header");auto u32=[&](size_t p){if(p+4>texture.size())throw std::runtime_error("texture bounds");return unsigned(texture[p])|(unsigned(texture[p+1])<<8)|(unsigned(texture[p+2])<<16)|(unsigned(texture[p+3])<<24);};
 unsigned dimensions=u32(8),width=dimensions&65535,height=dimensions>>16;if(width!=128||height!=128)throw std::runtime_error("texture dimensions");
 auto container=inflate(std::vector<unsigned char>(texture.begin()+40,texture.end()),1<<20);
 if(container.size()<12)throw std::runtime_error("texture container");auto pixels=inflate(std::vector<unsigned char>(container.begin()+12,container.end()),65536);if(pixels.size()!=65536)throw std::runtime_error("texture pixels");return pixels;
}
}
