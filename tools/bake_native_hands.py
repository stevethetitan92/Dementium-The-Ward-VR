"""Bake locally extracted native hands for independent controller rendering.
Only native thumb/index articulation is retained; other finger IK is not claimed.
"""
from pathlib import Path
import runpy,struct,json,hashlib,collections,shutil
import numpy as np
root=Path(__file__).resolve().parents[3];folder=Path(__file__).resolve().parents[1]/'assets/hands'
native=runpy.run_path(str(root/'work/build_native_hand_trial.py'));faces=native['faces'];point=native['point']
base=np.array([[point(b,xyz)for b,xyz,uv in t]for t in faces]);uv=np.array([[uv for b,xyz,uv in t]for t in faces])/128
edges=collections.Counter();oriented={}
for t in base:
 for a,b in[(0,1),(1,2),(2,0)]:
  A=tuple(np.round(t[a],5));B=tuple(np.round(t[b],5));key=tuple(sorted((A,B)));edges[key]+=1;oriented[key]=(np.array(A),np.array(B))
boundary=[oriented[e]for e,c in edges.items()if c==1 and min(p[2]for p in e)>-.025]
adj=collections.defaultdict(list)
for a,b in boundary:adj[tuple(a)].append(tuple(b));adj[tuple(b)].append(tuple(a))
assert len(boundary)==8 and all(len(v)==2 for v in adj.values())
loop=[next(iter(adj))];previous=None
while True:
 next_point=next(v for v in adj[loop[-1]]if v!=previous)
 if next_point==loop[0]:break
 previous=loop[-1];loop.append(next_point)
assert len(loop)==8;ring=np.array(loop);center=ring[:,:2].mean(axis=0)
def cap(positions):
 result=list(positions)
 for a,b in boundary:
  A=a.copy();B=b.copy();A[2]=B[2]=.028
  result.extend([[a,b,B],[a,B,A],[[center[0],center[1],.028],A,B]])
 return np.array(result)
extra_uv=np.tile([.125,.45],(len(boundary)*9,1)).reshape(-1,3,2);full_uv=np.concatenate([uv,extra_uv])
def stream(positions,side):
 p=cap(positions)
 if not side:p=p[:,[0,2,1]].copy();p[:,:,0]*=-1;tex=full_uv[:,[0,2,1]]
 else:tex=full_uv
 normal=np.cross(p[:,1]-p[:,0],p[:,2]-p[:,0]);normal/=np.maximum(np.linalg.norm(normal,axis=1)[:,None],1e-9)
 return np.c_[p.reshape(-1,3),np.repeat(normal,3,axis=0),np.ones((p.size//3,3)),tex.reshape(-1,2),np.full(p.size//3,5)].astype('<f4')
def sample(name,frame=0):
 raw=(root/'work/weapon-assets'/f'{name}.anm').read_bytes();frames=struct.unpack_from('<I',raw)[0];frame=min(frame,frames)
 return np.array([struct.unpack_from('<7h',raw,8+(frame*12+i)*14)for i in range(12)])/4096
rest=sample('9mm_load');fire_raw=(root/'work/weapon-assets/9mm_fire.anm').read_bytes();frames=struct.unpack_from('<I',fire_raw)[0]
# Select the strongest actual trigger-finger deviation in the native firing clip.
trigger=max(range(frames+1),key=lambda f:1-abs(np.dot(sample('9mm_fire',f)[4,:4],rest[4,:4])))
native_trigger=sample('9mm_fire',trigger)
def pose(values):return np.array([[point(b,xyz,values)for b,xyz,uv in t]for t in faces])
bank=[];names=[]
for name in['Relaxed','WeaponGrip','TriggerGrip','PillBottleGrip','MagazineGrip','ShellPinch','NightstickGrip','BuzzsawGrip']:
 p=pose(native_trigger if name=='TriggerGrip'else rest)
 for side in range(2):bank.append(stream(p,side));names.append(name+('Right'if side else'Left'))
old=(folder/'FloatingHands.bin').read_bytes();counts=struct.unpack_from('<36I',old,12);raw=np.frombuffer(old,dtype='<f4',offset=156).reshape(-1,12);offsets=np.r_[0,np.cumsum(counts)]
right_band=raw[offsets[17]:offsets[18]].copy();lo=right_band[:,:2].min(axis=0);hi=right_band[:,:2].max(axis=0);source_center=(lo+hi)/2;source_radius=(hi-lo)/2
def map_band(rows,side):
 rows=rows.copy();xy=rows[:,:2].astype(float);xy[:,0]*=1 if side else-1;v=(xy-source_center)/source_radius;theta=np.arctan2(v[:,1],v[:,0])
 for k,a in enumerate(theta):
  direction=np.array([np.cos(a),np.sin(a)]);radius=1.
  for i,A in enumerate(ring[:,:2]):
   B=ring[(i+1)%len(ring),:2];edge=B-A;matrix=np.c_[direction,-edge]
   if abs(np.linalg.det(matrix))<1e-10:continue
   t,u=np.linalg.solve(matrix,A-center)
   if t>0 and 0<=u<=1:radius=min(radius,t)
  source_r=1/np.linalg.norm(direction/source_radius);offset=np.linalg.norm(xy[k]-source_center)-source_r
  rows[k,:2]=center+direction*(radius+.0015+np.clip(offset,-.0005,.0005))
 rows[:,0]*=1 if side else-1;rows[:,2]+=.014
 return rows
for side in range(2):bank.append(map_band(raw[offsets[16+side]:offsets[17+side]],side));names.append(('Right'if side else'Left')+'WristHUD')
for step in range(5):
 for side in range(2):bank.append(stream(base,side));names.append(f'NativeMechanism{step}'+('Right'if side else'Left'))
for name in['ShotgunWeaponGrip','ShotgunTriggerGrip','SniperWeaponGrip','SniperTriggerGrip']:
 for side in range(2):bank.append(stream(pose(native_trigger if'Trigger'in name else rest),side));names.append(name+('Right'if side else'Left'))
assert len(bank)==36 and all(np.isfinite(a).all()and len(a)%3==0 for a in bank)
payload=b''.join(a.tobytes()for a in bank);(folder/'NativeHands.bin').write_bytes(struct.pack('<4sII',b'DHND',1,36)+struct.pack('<36I',*[len(a)for a in bank])+payload)
shutil.copy2(root/'work/9mm-texture.png',folder/'NativeHandAlbedo.png')
report={'source':'Actual locally extracted 9mm.msh and native firing animation','source_sha256':hashlib.sha256((root/'work/weapon-assets/9mm.msh').read_bytes()).hexdigest(),'native_hand_triangles':len(base),'wrist_boundary_vertices':ring.tolist(),'wrist_end_metres':.028,'band_source_center':source_center.tolist(),'band_source_radius':source_radius.tolist(),'native_wrist_center':center.tolist(),'trigger_frame':trigger,'sections':[{'name':n,'vertices':len(a)}for n,a in zip(names,bank)],'limitations':['Native thumb/index motion only; no new full finger IK','Non-trigger item poses currently share the native closed grip; contact fitting pending']}
(folder/'native-hand-fit.json').write_text(json.dumps(report,indent=2));print('Baked native original-hand trial:',len(payload),'bytes; 36 streams; original anatomy and thumb/index motion retained')
def number(v):return f'{float(v):.9f}f'
header='''#pragma once
#include <cmath>
#include <algorithm>
namespace nativeBandFit {
// Generated from the actual native hand's eight-edge wrist opening.
'''
header+='inline constexpr float ring[8][2]={'+','.join('{'+','.join(number(v)for v in row[:2])+'}'for row in ring)+'};\n'
for name,values in[('center',center),('sourceCenter',source_center),('sourceRadius',source_radius)]:header+='inline constexpr float '+name+'[2]={'+','.join(number(v)for v in values)+'};\n'
header+='''inline void map(float& x,float& y,float& z,unsigned side){
 float sign=side?1.f:-1.f,px=x*sign,py=y;
 float angle=std::atan2((py-sourceCenter[1])/sourceRadius[1],(px-sourceCenter[0])/sourceRadius[0]);
 float dx=std::cos(angle),dy=std::sin(angle),radius=1;
 for(unsigned i=0;i<8;i++){float ax=ring[i][0]-center[0],ay=ring[i][1]-center[1],ex=ring[(i+1)%8][0]-ring[i][0],ey=ring[(i+1)%8][1]-ring[i][1];
  float det=dx*ey-dy*ex;if(std::fabs(det)<1e-10f)continue;
  float t=(ax*ey-ay*ex)/det,u=(ax*dy-ay*dx)/det;if(t>0&&u>=0&&u<=1)radius=(std::min)(radius,t);
 }
 float sourceR=1/std::hypot(dx/sourceRadius[0],dy/sourceRadius[1]);
 float offset=std::clamp(std::hypot(px-sourceCenter[0],py-sourceCenter[1])-sourceR,-.0005f,.0005f);
 x=sign*(center[0]+dx*(radius+.0015f+offset));y=center[1]+dy*(radius+.0015f+offset);z+=.014f;
}
}
'''
(folder.parents[1]/'src/native_band_fit.hpp').write_text(header)
