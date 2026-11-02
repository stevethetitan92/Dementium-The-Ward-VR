#pragma once
#include <d3d12.h>
#include <array>
namespace descriptorBindings {
struct State {
    std::array<const void*,2> descriptorHeaps{};
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE,16> textures{};
    void setHeaps(const std::array<const void*,2>& next) {
        // Heap changes invalidate table arguments. Rebinding the same heaps
        // is redundant and preserves arguments, regardless of array order.
        if(descriptorHeaps!=next){textures={};descriptorHeaps=next;}
    }
};
}
