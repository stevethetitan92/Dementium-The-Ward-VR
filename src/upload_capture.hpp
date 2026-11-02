#pragma once
#include <cstddef>
#include <cstring>
#include <vector>
#include <utility>
#include <algorithm>

// CPU capture metadata only; this does not synchronize GPU work.
struct UploadCapture {
    unsigned char* cpu{};
    size_t size{};
    unsigned mappings{};
    std::vector<unsigned char> shadow;
    std::vector<std::pair<size_t,size_t>> known;
    void dimensions(size_t bytes){
        if(bytes!=size){shadow.clear();known.clear();size=bytes;}
    }
    void remember(size_t first,size_t last){
        if(first>=last)return;
        auto it=known.begin();while(it!=known.end()&&it->second<first)++it;
        while(it!=known.end()&&it->first<=last){first=(std::min)(first,it->first);last=(std::max)(last,it->second);it=known.erase(it);}
        known.insert(it,{first,last});
    }
    bool canRead(size_t offset,size_t bytes) const {
        if(!cpu||offset>size||bytes>size-offset)return false;
        if(mappings||!bytes)return true;
        for(const auto& range:known){if(range.first>offset)return false;if(offset<=range.second&&bytes<=range.second-offset)return true;}
        return false;
    }
    void mapped(void* pointer,size_t bytes) {
        dimensions(bytes);
        // D3D12 permits ppData=nullptr and counts that Map as well. A nested
        // pointerless Map must not discard the earlier still-live address.
        if(pointer)cpu=static_cast<unsigned char*>(pointer);
        else if(!mappings)cpu=nullptr;
        ++mappings;
    }
    void unmapped(size_t first,size_t last) {
        if(!mappings)return;
        if(cpu&&size<=64*1024*1024&&first<=last&&last<=size) {
            shadow.resize(size);
            if(last>first){std::memcpy(shadow.data()+first,cpu+first,last-first);remember(first,last);}
        }
        if(--mappings==0)cpu=shadow.size()==size?shadow.data():nullptr;
    }
    void copied(size_t bytes,size_t offset,const unsigned char* source,size_t count) {
        // A mapped upload remains authoritative, even if it has an old shadow.
        if(mappings||bytes>64*1024*1024||offset>bytes||count>bytes-offset)return;
        dimensions(bytes);shadow.resize(size);
        if(count){std::memcpy(shadow.data()+offset,source,count);remember(offset,offset+count);}
        cpu=shadow.data();
    }
};
