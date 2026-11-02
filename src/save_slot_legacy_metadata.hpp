#pragma once
#include <filesystem>
namespace saveSlots {
// Legacy metadata is optional on a fresh install. Other filesystem errors must
// remain visible to initialization diagnostics, never masquerade as empty saves.
inline void importLegacyMedicine(const std::filesystem::path& source,const std::filesystem::path& destination){
 namespace fs=std::filesystem;
 std::error_code ec;
 fs::directory_iterator it(source,ec),end;
 if(ec==std::errc::no_such_file_or_directory)return;
 if(ec)throw fs::filesystem_error("Open legacy medicine metadata",source,ec);
 for(;it!=end;it.increment(ec)){
  if(ec)throw fs::filesystem_error("Enumerate legacy medicine metadata",source,ec);
  const auto& e=*it;
  auto name=e.path().filename().wstring();
  if(e.is_regular_file()&&!e.is_symlink()&&name.find(L".vrmeds")!=std::wstring::npos)
   fs::copy_file(e.path(),destination/e.path().filename(),fs::copy_options::skip_existing);
 }
 if(ec)throw fs::filesystem_error("Enumerate legacy medicine metadata",source,ec);
}
}
