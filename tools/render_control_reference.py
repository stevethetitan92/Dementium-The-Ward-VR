"""Offline orthographic renders of the installed SteamVR Touch Plus models.
No game, tracking, or GPU state is touched. Assets remain local to this build.
"""
from pathlib import Path
import json
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
SOURCE=Path('C:/Program Files (x86)/Steam/steamapps/common/SteamVR/resources/rendermodels')
for hand in ('left','right'):
    name='oculus_quest_plus_controller_'+hand
    folder=SOURCE/name
    verts=[];uv=[];normals=[];faces=[]
    for line in (folder/(name+'.obj')).read_text().splitlines():
        bits=line.split()
        if not bits:continue
        if bits[0]=='v':verts.append([float(v) for v in bits[1:4]])
        elif bits[0]=='vt':uv.append([float(v) for v in bits[1:3]])
        elif bits[0]=='vn':normals.append([float(v) for v in bits[1:4]])
        elif bits[0]=='f':
            poly=[[int(v)-1 for v in b.split('/')] for b in bits[1:]]
            faces.extend([[poly[0],poly[i],poly[i+1]] for i in range(1,len(poly)-1)])
    verts=np.array(verts);uv=np.array(uv);faces=np.array(faces);normals=np.array(normals)
    direction=np.array([-.25 if hand=='left' else .25,.92,.38]);direction/=np.linalg.norm(direction)
    right=np.cross([0,1,0],direction);right/=np.linalg.norm(right)
    up=np.cross(direction,right)
    p=verts@np.array([right,up,direction]).T
    scale=650/max(np.ptp(p[:,0]),np.ptp(p[:,1]))
    center=(p[:,:2].min(axis=0)+p[:,:2].max(axis=0))/2
    p[:,:2]-=center
    p[:,0]=p[:,0]*scale+400;p[:,1]=400-p[:,1]*scale
    tex=np.asarray(Image.open(folder/(name+'_color.png')).convert('RGB'))
    out=np.zeros((800,800,4),dtype=np.uint8);depth=np.full((800,800),-np.inf)
    light=np.array([-.3,.8,.6]);light/=np.linalg.norm(light)
    for face in faces:
        ids=face[:,0];a,b,c=p[ids]
        x0,y0=np.maximum(np.floor(np.min(p[ids,:2],axis=0)).astype(int),0)
        x1,y1=np.minimum(np.ceil(np.max(p[ids,:2],axis=0)).astype(int),799)
        if x1<x0 or y1<y0:continue
        xx,yy=np.meshgrid(np.arange(x0,x1+1)+.5,np.arange(y0,y1+1)+.5)
        den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
        if abs(den)<1e-8:continue
        w0=((b[1]-c[1])*(xx-c[0])+(c[0]-b[0])*(yy-c[1]))/den
        w1=((c[1]-a[1])*(xx-c[0])+(a[0]-c[0])*(yy-c[1]))/den;w2=1-w0-w1
        z=w0*a[2]+w1*b[2]+w2*c[2]
        target=depth[y0:y1+1,x0:x1+1];mask=(w0>=0)&(w1>=0)&(w2>=0)&(z>target)
        if not np.any(mask):continue
        t=uv[face[:,1]];s=w0[...,None]*t[0]+w1[...,None]*t[1]+w2[...,None]*t[2]
        tx=np.clip((s[:,:,0]*(tex.shape[1]-1)).astype(int),0,tex.shape[1]-1)
        ty=np.clip(((1-s[:,:,1])*(tex.shape[0]-1)).astype(int),0,tex.shape[0]-1)
        ns=normals[face[:,2]];n=w0[...,None]*ns[0]+w1[...,None]*ns[1]+w2[...,None]*ns[2]
        n/=np.maximum(np.linalg.norm(n,axis=2,keepdims=True),1e-12)
        shading=.72+.28*np.abs(n@light)
        patch=out[y0:y1+1,x0:x1+1];patch[mask,:3]=np.clip(tex[ty,tx][mask]*shading[mask,None],0,255).astype('uint8');patch[mask,3]=255;target[mask]=z[mask]
    image=Image.fromarray(out);bounds=image.getbbox();image=image.crop(bounds)
    image.save(ROOT/'assets/inventory'/('quest-touch-plus-'+hand+'.png'))
    anchors={}
    for key,component in json.loads((folder/(name+'.json')).read_text())['components'].items():
        if 'component_local' not in component:continue
        pt=np.array(component['component_local']['origin'])@np.array([right,up,direction]).T
        anchors[key]=[(float(pt[0])-center[0])*scale+400-bounds[0],400-(float(pt[1])-center[1])*scale-bounds[1]]
    (ROOT/'assets/inventory'/('quest-touch-plus-'+hand+'.json')).write_text(json.dumps(anchors,indent=2))
    print(hand,len(faces),image.size)
