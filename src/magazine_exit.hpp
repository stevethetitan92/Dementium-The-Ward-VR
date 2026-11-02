#pragma once
#include "selected_frames.hpp"
#include "reload_alignment.hpp"
namespace magazineExit {
using namespace DirectX;
inline std::array<XrVector3f,7> travel{};
// Fit the magazine's long axis in gun space. Keep its authored angle while it
// leaves the well; only the subsequent free-flight phase may tumble.
inline bool bind(const psxAssets::Pack& pack,unsigned weapon,unsigned section){
 if(weapon>=7)return false;travel[weapon]={};manualReload::magazineEntryClearance[weapon]=0;
 if(section>=pack.sections.size()||!selectedFrames::ready)return false;
 const auto& s=pack.sections[section];if(s.count<3||s.first+s.count>pack.vertices.size())return false;
 const auto frame=XMLoadFloat4x4(&selectedFrames::frames[weapon].editorToAim);
 std::vector<XMFLOAT3> points;XMFLOAT3 mean{};
 for(unsigned i=0;i<s.count;++i){auto v=pack.vertices[s.first+i].value;XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformCoord(XMVectorSet(v[0],v[1],v[2],1),frame));points.push_back(p);mean.x+=p.x;mean.y+=p.y;mean.z+=p.z;}
 mean.x/=s.count;mean.y/=s.count;mean.z/=s.count;
 float c[3][3]{};for(auto p:points){float v[]={p.x-mean.x,p.y-mean.y,p.z-mean.z};for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)c[i][j]+=v[i]*v[j];}
 XMFLOAT3 axis{0,-1,0};
 for(unsigned iteration=0;iteration<32;++iteration){float a[]={axis.x,axis.y,axis.z},v[3]{};for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)v[i]+=c[i][j]*a[j];float length=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);if(!std::isfinite(length)||length<1e-8f)return false;axis={v[0]/length,v[1]/length,v[2]/length};}
 if(axis.y>0){axis.x=-axis.x;axis.y=-axis.y;axis.z=-axis.z;}
 // The M16 lower magazine curves away from its straight upper insertion
 // section. Whole-mesh PCA tilts the rail into the receiver wall. The source
 // upper section and well are vertical in the saved gun frame. The hunting
 // box is also vertical but wider front-to-back than tall; neither magazine's
 // longest whole-mesh dimension describes its insertion rail.
 if(weapon==2||weapon==4)axis={0,-1,0};
 if(axis.y>-.6f)return false;
 float low=10,high=-10;for(auto p:points){float d=p.x*axis.x+p.y*axis.y+p.z*axis.z;low=(std::min)(low,d);high=(std::max)(high,d);}
 const float distance=high-low+.01f;if(distance<.035f||distance>.35f)return false;
 XMFLOAT3 editor;XMStoreFloat3(&editor,XMVector3TransformNormal(XMVectorSet(axis.x*distance,axis.y*distance,axis.z*distance,0),XMMatrixInverse(nullptr,frame)));
 travel[weapon]={editor.x,editor.y,editor.z};manualReload::magazineExitAxes[weapon]={axis.x,axis.y,axis.z};manualReload::magazineEntryClearance[weapon]=distance;return true;
}
inline constexpr float duration=.18f;
inline float fraction(float seconds){return std::clamp(seconds/duration,0.f,1.f);}
}
