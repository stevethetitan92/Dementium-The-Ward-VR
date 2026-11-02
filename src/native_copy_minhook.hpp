#pragma once
#include "native_copy_install.hpp"
#include <MinHook.h>
namespace nativeCopyMinHook {
// Unused backend. MinHook lifetime and installation quiescence are owned by
// the caller; only explicitly owned targets are enabled/removed.
struct Backend {
    bool create(const nativeCopyInstall::Binding& binding){return MH_CreateHook(binding.target,binding.detour,binding.original)==MH_OK;}
    bool enable(void* target){auto result=MH_EnableHook(target);return result==MH_OK||result==MH_ERROR_ENABLED;}
    bool disable(void* target)noexcept{auto result=MH_DisableHook(target);return result==MH_OK||result==MH_ERROR_DISABLED||result==MH_ERROR_NOT_CREATED;}
    bool remove(void* target)noexcept{auto result=MH_RemoveHook(target);return result==MH_OK||result==MH_ERROR_NOT_CREATED;}
};
}
