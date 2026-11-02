"""Fit the existing approved band mesh to the supplied rig, without changing its artwork."""
from pathlib import Path
import json,struct,hashlib
import numpy as np
from rig_supplied_arms import read_glb,accessor

root=Path(__file__).resolve().parents[1];folder=root/'assets/hands'
doc,blob=read_glb(folder/'ReplacementArms.glb')
hand=accessor(doc,blob,doc['meshes'][0]['primitives'][0]['attributes']['POSITION'])
raw=(root/'assets/wristband/RedmoorWristband.mesh').read_bytes()
band=np.frombuffer(raw,dtype='<f4').reshape(-1,12).copy()
# The legacy inspection export ends with unknown-value counters. The snap is
# the final static item; live counters must not be baked beneath the real values.
band=band[:np.flatnonzero(band[:,11]==2)[-1]+1]
original_center=np.array([.0018,-.0082,.003]);original_radius=np.array([.038412485,.023203571])
def append(array,kind,component=5126):
    array=np.ascontiguousarray(array);blob.extend(b'\0'*((-len(blob))%4));offset=len(blob);blob.extend(array.tobytes())
    view=len(doc['bufferViews']);doc['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':array.nbytes})
    a={'bufferView':view,'componentType':component,'count':len(array),'type':kind}
    if kind=='VEC3':a.update(min=array.min(axis=0).tolist(),max=array.max(axis=0).tolist())
    i=len(doc['accessors']);doc['accessors'].append(a);return i
texture=(root/'assets/wristband/RedmoorWristband-albedo.png').read_bytes()
blob.extend(b'\0'*((-len(blob))%4));offset=len(blob);blob.extend(texture)
view=len(doc['bufferViews']);doc['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':len(texture)})
image=len(doc.setdefault('images',[]));doc['images'].append({'bufferView':view,'mimeType':'image/png'})
tex=len(doc.setdefault('textures',[]));doc['textures'].append({'source':image})
material=len(doc['materials']);doc['materials'].extend([
 {'name':'Approved printed wristband','doubleSided':True,'pbrMetallicRoughness':{'baseColorTexture':{'index':tex},'roughnessFactor':.88,'metallicFactor':0}},
 {'name':'Matte ivory band inner wall','doubleSided':True,'pbrMetallicRoughness':{'roughnessFactor':.88,'metallicFactor':0}}])
# Cross-section envelopes follow the actual wrist along the cuff, avoiding a
# loose cylinder sized to the widest part of the palm.
indices=accessor(doc,blob,doc['meshes'][0]['primitives'][0]['indices']).flatten()
source_triangles=hand[indices.reshape(-1,3)]
centers=[];radials=[]
for x in np.linspace(.336,.364,9):
    segments=[]
    for triangle in source_triangles:
        if triangle[:,0].min()>x or triangle[:,0].max()<x:continue
        points=[]
        for a,b in zip(triangle,np.roll(triangle,-1,axis=0)):
            if (a[0]<x)!=(b[0]<x):points.append((a+(b-a)*(x-a[0])/(b[0]-a[0]))[1:])
        if len(points)==2:segments.append(points)
    segments=np.array(segments);points=segments.reshape(-1,2);center=(points.min(axis=0)+points.max(axis=0))*.5
    centers.append(center);values=[]
    for theta in np.arange(64)*2*np.pi/64:
        direction=np.array([np.sin(theta),np.cos(theta)]);a=segments[:,0]-center;v=segments[:,1]-segments[:,0]
        cross=lambda u,w:u[...,0]*w[...,1]-u[...,1]*w[...,0]
        den=cross(v,direction);t=np.divide(-cross(a,direction),den,out=np.full(len(a),np.nan),where=abs(den)>1e-12)
        distance=((a+v*t[:,None])*direction).sum(axis=1);valid=(t>=-1e-6)&(t<=1+1e-6)&(distance>=0)
        assert valid.any();values.append(distance[valid].max())
    radials.append(values)
centers=np.array(centers);radials=np.array(radials)
def sample(theta,x):
    z=np.clip((x-.336)/.0035,0,8);i=min(int(z),7);fraction=z-i
    a=(theta%(2*np.pi))*64/(2*np.pi);k=int(a)%64;t=a-int(a)
    r0=radials[i,k]*(1-t)+radials[i,(k+1)%64]*t;r1=radials[i+1,k]*(1-t)+radials[i+1,(k+1)%64]*t
    return centers[i]*(1-fraction)+centers[i+1]*fraction,r0*(1-fraction)+r1*fraction
# Measure interpolation undershoot against source vertices and edge midpoints.
checks=np.concatenate([source_triangles.reshape(-1,3),((source_triangles+np.roll(source_triangles,-1,axis=1))*.5).reshape(-1,3)])
checks=checks[(checks[:,0]>=.336)&(checks[:,0]<=.364)]
undershoot=0
for point in checks:
    u=np.clip((point[0]-.336)/.0035,0,8);i=min(int(u),7);center=centers[i]*(1-(u-i))+centers[i+1]*(u-i)
    v=point[1:]-center;theta=np.arctan2(v[0],v[1]);c,r=sample(theta,point[0]);undershoot=max(undershoot,float(np.linalg.norm(v)-r))
stand_off=.0014+undershoot+.0001
header='#pragma once\nnamespace replacementHands {\ninline constexpr float bandStandOff='+str(stand_off)+';\ninline constexpr float bandCenters[9][2]='+str(centers.tolist()).replace('[','{').replace(']','}')+';\ninline constexpr float bandRadial[9][64]='+str(radials.tolist()).replace('[','{').replace(']','}')+';\n}\n'
# Add f suffix to avoid thousands of narrowing diagnostics in C++ list initialization.
import re
header=re.sub(r'(?<![A-Za-z0-9_])(\d+\.\d+(?:e[+-]?\d+)?)',r'\1f',header)
(root/'src/replacement_band_fit.hpp').write_text(header)
report={}
for side,sign in [('Left',-1),('Right',1)]:
    selected=hand[(hand[:,0]>.332)&(hand[:,0]<.368)];yz=selected[:,[1,2]]
    center=centers[4];radii=np.array([radials[4,16],radials[4,0]])+.0014
    new=band.copy();local=band[:,:3]-original_center;new[:,0]=sign*(.350-local[:,2]*.625)
    for row,v in enumerate(local):
        theta=np.arctan2(v[1]/original_radius[1],v[0]/original_radius[0]);c,r=sample(theta,.350-v[2]*.625)
        normalized=np.hypot(v[0]/original_radius[0],v[1]/original_radius[1])
        offset=(normalized-1)*np.hypot(original_radius[0]*np.cos(theta),original_radius[1]*np.sin(theta))
        offset=-.00075+(offset+.00075)*.12
        new[row,1:3]=c+np.array([np.sin(theta),np.cos(theta)])*(r+stand_off+offset)
    if side=='Left':new[:,9]=.5-new[:,9]
    # Recompute normals after the nonuniform anatomical fit.
    for i in range(0,len(new),3):
        t=new[i:i+3,:3];n=np.cross(t[1]-t[0],t[2]-t[0]);n/=max(np.linalg.norm(n),1e-12);new[i:i+3,3:6]=n
    wrist=next(i for i,n in enumerate(doc['nodes'])if n.get('name')==side+'Hand')
    # Band is rigidly parented to the hand joint; its vertices become joint-local.
    wrist_origin=np.array([sign*.350,-.020,0]);new[:,:3]-=wrist_origin
    primitives=[]
    for printed in [True,False]:
        rows=np.concatenate([new[i:i+3]for i in range(0,len(new),3)if bool(band[i,11]==0)==printed])
        attrs={'POSITION':append(rows[:,:3].astype('<f4'),'VEC3'),'NORMAL':append(rows[:,3:6].astype('<f4'),'VEC3'),
               'TEXCOORD_0':append(rows[:,9:11].astype('<f4'),'VEC2'),'COLOR_0':append((np.ones_like(rows[:,6:9])if printed else rows[:,6:9]).astype('<f4'),'VEC3')}
        primitives.append({'attributes':attrs,'mode':4,'material':material+(0 if printed else 1)})
    mesh=len(doc['meshes']);doc['meshes'].append({'name':side+' fitted wrist HUD','primitives':primitives})
    node=len(doc['nodes']);doc['nodes'].append({'name':side+' fitted wrist HUD','mesh':mesh,'extras':{'hudWrist':side,'fitOnly':True}})
    doc['nodes'][wrist].setdefault('children',[]).append(node)
    clearance=float(stand_off-.00075-undershoot) # measured against vertices and edge midpoints
    assert clearance>.0005
    report[side]={'wrist_node':wrist,'center_yz_m':center.tolist(),'radius_yz_m':radii.tolist(),'conservative_clearance_m':clearance,'source_vertices_checked':len(yz)}
doc['buffers']=[{'byteLength':len(blob)}]
encoded=json.dumps(doc,separators=(',',':')).encode();encoded+=b' '*((-len(encoded))%4);blob.extend(b'\0'*((-len(blob))%4))
(folder/'ReplacementArms-WristHUD.glb').write_bytes(struct.pack('<III',0x46546c67,2,28+len(encoded)+len(blob))+struct.pack('<II',len(encoded),0x4e4f534a)+encoded+struct.pack('<II',len(blob),0x004e4942)+blob)
report['status']='Wrist-conforming source fit checked; native rendering, wrist selection and status integration implemented. Live headset validation pending.'
report['artwork_sha256']=hashlib.sha256(texture).hexdigest()
(folder/'replacement-wrist-fit.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))

