"""Bake the validated floating rig into bounded native-renderer pose streams."""
from pathlib import Path
import io,json,struct
import numpy as np
from PIL import Image
from rig_supplied_arms import read_glb,accessor
folder=Path(__file__).resolve().parents[1]/'assets/hands'
doc,blob=read_glb(folder/'FloatingHands-WristHUD.glb');primitive=doc['meshes'][0]['primitives'][0];attributes=primitive['attributes']
p=accessor(doc,blob,attributes['POSITION']);normals=accessor(doc,blob,attributes['NORMAL']);uv=accessor(doc,blob,attributes['TEXCOORD_0'])
weights=accessor(doc,blob,attributes['WEIGHTS_0']);joints=accessor(doc,blob,attributes['JOINTS_0']);skin=doc['skins'][0]
inverse=accessor(doc,blob,skin['inverseBindMatrices']).reshape(-1,4,4).transpose(0,2,1)
parent={c:i for i,n in enumerate(doc['nodes'])for c in n.get('children',[])}
material=doc['materials'][primitive['material']]['pbrMetallicRoughness'];texture=doc['textures'][material['baseColorTexture']['index']];image=doc['images'][texture['source']];view=doc['bufferViews'][image['bufferView']]
pixels=np.asarray(Image.open(io.BytesIO(blob[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']])).convert('RGB'))/255
xy=np.floor((uv%1)*[pixels.shape[1],pixels.shape[0]]).astype(int);color=np.tile(np.array(material.get('baseColorFactor',[1,1,1,1])[:3]),(len(p),1))
# Keep the supplied diffuse texture, rather than flattening it into vertex colours.
(folder/'HandAlbedo.png').write_bytes(bytes(blob[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']]))
def transforms(rotations):
    world={}
    def visit(i):
        if i in world:return world[i]
        node=doc['nodes'][i];m=np.eye(4);m[:3,3]=node.get('translation',[0,0,0]);x,y,z,w=rotations.get(i,[0,0,0,1])
        m[:3,:3]=[[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]]
        world[i]=visit(parent[i])@m if i in parent else m;return world[i]
    return np.array([visit(i)@inverse[k]for k,i in enumerate(skin['joints'])])
def canonical(points,side,normal=False):
    sign=1 if side else -1;q=points.copy()
    if not normal:q-=np.array([sign*.350,-.020,0])
    return np.c_[sign*q[:,2],q[:,1],-sign*q[:,0]]
hand_scale=1.6 # adult hand length and wrist dimensions in physical metres
sections=[];labels=[]
for animation in doc['animations']:
    rotations={c['target']['node']:accessor(doc,blob,animation['samplers'][c['sampler']]['output'])[-1]for c in animation['channels']}
    matrices=transforms(rotations);out=np.zeros_like(p);outn=np.zeros_like(normals)
    for slot in range(4):
        m=matrices[joints[:,slot]];out+=np.einsum('nij,nj->ni',m,np.c_[p,np.ones(len(p))])[:,:3]*weights[:,slot,None]
        outn+=np.einsum('nij,nj->ni',m[:,:3,:3],normals)*weights[:,slot,None]
    outn/=np.maximum(np.linalg.norm(outn,axis=1)[:,None],1e-8)
    for side in range(2):
        mask=p[:,0]>0 if side else p[:,0]<0
        data=np.c_[canonical(out[mask],side)*hand_scale,canonical(outn[mask],side,True),color[mask],uv[mask],np.full(mask.sum(),5)].astype('<f4')
        sections.append(data);labels.append(animation['name']+('Right'if side else'Left'))
for side,name in enumerate(['Left','Right']):
    node=next(n for n in doc['nodes']if n.get('name')==name+' fitted wrist HUD');rows=[]
    for prim in doc['meshes'][node['mesh']]['primitives']:
        a=prim['attributes'];pos=accessor(doc,blob,a['POSITION']);normal=accessor(doc,blob,a['NORMAL']);tex=accessor(doc,blob,a['TEXCOORD_0']);colors=accessor(doc,blob,a['COLOR_0'])
        # Band vertices are already wrist joint local.
        sign=1 if side else -1;xyz=np.c_[sign*pos[:,2],pos[:,1],-sign*pos[:,0]];n=np.c_[sign*normal[:,2],normal[:,1],-sign*normal[:,0]]
        printed='baseColorTexture'in doc['materials'][prim['material']]['pbrMetallicRoughness']
        rows.append(np.c_[xyz*hand_scale,n,colors,tex,np.full(len(pos),0 if printed else -1)].astype('<f4'))
    sections.append(np.concatenate(rows));labels.append(name+'WristHUD')
assert len(sections)==18 and all(len(s)%3==0 and np.isfinite(s).all()for s in sections)
# Retarget donor finger bends during its bolt cycle. Wrist trajectories are
# deliberately not copied: physical tracked hands own those transforms in VR.
donor=Path('C:/Users/YOUR_USERNAME/Downloads/sniper_fps_animation.glb')
retarget=[]
if not donor.exists():raise FileNotFoundError('The supplied sniper animation is required to rebuild the full hand bank')
if donor.exists():
    source,source_blob=read_glb(donor);clip=source['animations'][0]
    tracks={}
    for channel in clip['channels']:
        if channel['target']['path']!='rotation':continue
        sample=clip['samplers'][channel['sampler']]
        tracks[channel['target']['node']]=(accessor(source,source_blob,sample['input']).ravel(),accessor(source,source_blob,sample['output']))
    def sample_track(track,time):
        times,values=track;i=int(np.clip(np.searchsorted(times,time)-1,0,len(times)-2));a=values[i].copy();b=values[i+1].copy()
        if np.dot(a,b)<0:b=-b
        t=np.clip((time-times[i])/max(times[i+1]-times[i],1e-8),0,1);q=a*(1-t)+b*t;return q/max(np.linalg.norm(q),1e-8)
    for step,time in enumerate(np.linspace(6.80,7.90,5)):
        rotations={};angles={}
        for node in skin['joints']:
            name=doc['nodes'][node]['name'];side='Right'if name.startswith('Right')else'Left';tail=name[len(side):]
            if not any(tail.startswith(f)for f in ['Thumb','Index','Middle','Ring','Pinky']):continue
            prefix='mixamorig2:RightHand'+tail+'_' # the donor operates its bolt with the right hand
            source_node=next((i for i,n in enumerate(source['nodes'])if n.get('name','').startswith(prefix)),None)
            if source_node not in tracks:continue
            reference=sample_track(tracks[source_node],.0166667);current=sample_track(tracks[source_node],time)
            bend=float(np.clip((.65 if tail.startswith('Thumb')else .80)+.4*2*np.arccos(np.clip(abs(np.dot(reference,current)),0,1)),.5,1.5))
            # The donor magnitude describes a finger curl, not an angle to apply
            # in full at every joint. Match the repaired primary pose distribution.
            bend*=([.55,.65,.45] if tail.startswith('Thumb')else [.7,.8,.45])[int(tail[-1])-1]
            if side=='Right':bend=-bend
            rotations[node]=[0,0,np.sin(bend/2),np.cos(bend/2)]
            if tail=='Thumb1':
                y=(-1 if side=='Right'else 1)*1.04719755
                rotations[node]=[np.sin(y/2)*np.sin(bend/2),np.sin(y/2)*np.cos(bend/2),np.cos(y/2)*np.sin(bend/2),np.cos(y/2)*np.cos(bend/2)]
            angles[name]=bend
        matrices=transforms(rotations);out=np.zeros_like(p);outn=np.zeros_like(normals)
        for slot in range(4):
            m=matrices[joints[:,slot]];out+=np.einsum('nij,nj->ni',m,np.c_[p,np.ones(len(p))])[:,:3]*weights[:,slot,None]
            outn+=np.einsum('nij,nj->ni',m[:,:3,:3],normals)*weights[:,slot,None]
        outn/=np.maximum(np.linalg.norm(outn,axis=1)[:,None],1e-8)
        for side in range(2):
            mask=p[:,0]>0 if side else p[:,0]<0
            sections.append(np.c_[canonical(out[mask],side)*hand_scale,canonical(outn[mask],side,True),color[mask],uv[mask],np.full(mask.sum(),5)].astype('<f4'))
            labels.append('DonorBolt'+str(step)+('Right'if side else'Left'))
        retarget.append({'source_seconds':float(time),'finger_bend_radians':angles})
    (folder/'donor-bolt-finger-retarget.json').write_text(json.dumps({'method':'Bend magnitude transfer into target finger flexion axes; five bolt-travel samples; no donor wrist or magazine motion','samples':retarget},indent=2))
# Stock-neck variants counter-rotate the index MCP joint so the idle finger
# follows the barrel and the flexed finger meets the trigger, not the floor.
for angle in [np.pi/4,np.deg2rad(50)]:
    for clip_name in ['WeaponGrip','TriggerGrip']:
        animation=next(a for a in doc['animations']if a['name']==clip_name)
        rotations={c['target']['node']:accessor(doc,blob,animation['samplers'][c['sampler']]['output'])[-1]for c in animation['channels']}
        for node in skin['joints']:
            name=doc['nodes'][node]['name']
            if not name.endswith('Index1'):continue
            sign=1 if name.startswith('Right')else -1;y=sign*angle;z=-sign*.25
            rotations[node]=[np.sin(y/2)*np.sin(z/2),np.sin(y/2)*np.cos(z/2),np.cos(y/2)*np.sin(z/2),np.cos(y/2)*np.cos(z/2)]
        matrices=transforms(rotations);out=np.zeros_like(p);outn=np.zeros_like(normals)
        for slot in range(4):
            m=matrices[joints[:,slot]];out+=np.einsum('nij,nj->ni',m,np.c_[p,np.ones(len(p))])[:,:3]*weights[:,slot,None]
            outn+=np.einsum('nij,nj->ni',m[:,:3,:3],normals)*weights[:,slot,None]
        outn/=np.maximum(np.linalg.norm(outn,axis=1)[:,None],1e-8)
        for side in range(2):
            mask=p[:,0]>0 if side else p[:,0]<0
            sections.append(np.c_[canonical(out[mask],side)*hand_scale,canonical(outn[mask],side,True),color[mask],uv[mask],np.full(mask.sum(),5)].astype('<f4'))
            labels.append(('Shotgun'if angle<.8 else'Sniper')+clip_name+('Right'if side else'Left'))
payload=b''.join(s.tobytes()for s in sections)
header_size=12+4*len(sections)
(folder/'FloatingHands.bin').write_bytes(struct.pack('<4sII',b'DHND',1,len(sections))+struct.pack('<'+str(len(sections))+'I',*[len(s)for s in sections])+payload)
(folder/'runtime-poses.json').write_text(json.dumps({'format':'DHND v1: bounded triangle streams, 12 float32 per vertex','sections':[{'name':n,'vertices':len(s)}for n,s in zip(labels,sections)],'bytes':len(payload)+header_size},indent=2))
print('Baked',len(sections),'pose/HUD streams:',len(payload)+header_size,'bytes')
