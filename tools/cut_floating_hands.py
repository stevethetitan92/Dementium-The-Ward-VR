"""Clip the actual skinned mesh flush to the wrist HUD and mirror a matched pair.
Interpolates skin weights at the cut, seals the wrist, and retains every grip clip.
"""
from pathlib import Path
import json,struct
import numpy as np
from rig_supplied_arms import read_glb,accessor

folder=Path(__file__).resolve().parents[1]/'assets/hands'
doc,blob=read_glb(folder/'ReplacementArms-WristHUD.glb')
prim=doc['meshes'][0]['primitives'][0];attrs=prim['attributes'];skin=doc['skins'][0]
arrays={k:accessor(doc,blob,v)for k,v in attrs.items()};indices=accessor(doc,blob,prim['indices']).flatten()
hud=next(n for n in doc['nodes']if n.get('name')=='Right fitted wrist HUD')
bandpoints=np.concatenate([accessor(doc,blob,p['attributes']['POSITION'])for p in doc['meshes'][hud['mesh']]['primitives']])
cut=float((bandpoints[:,0]+.350).min())
def point(i):return {k:v[i].astype(float).copy()for k,v in arrays.items()}
def mix(a,b,t):
    out={k:a[k]*(1-t)+b[k]*t for k in a if k not in ('JOINTS_0','WEIGHTS_0')}
    weights={}
    for vertex,factor in [(a,1-t),(b,t)]:
        for j,w in zip(vertex['JOINTS_0'],vertex['WEIGHTS_0']):weights[int(j)]=weights.get(int(j),0)+w*factor
    selected=sorted(weights.items(),key=lambda v:-v[1])[:4];selected+= [(0,0)]*(4-len(selected))
    out['JOINTS_0']=np.array([j for j,w in selected]);w=np.array([w for j,w in selected]);out['WEIGHTS_0']=w/w.sum()
    out['NORMAL']/=max(np.linalg.norm(out['NORMAL']),1e-12)
    return out
triangles=[];boundary=[]
for ids in indices.reshape(-1,3):
    polygon=[point(i)for i in ids]
    # Use the actual supplied right hand as the common anatomical source.
    if max(p['POSITION'][0]for p in polygon)<cut:continue
    clipped=[]
    for a,b in zip(polygon,polygon[1:]+polygon[:1]):
        a_in=a['POSITION'][0]>=cut;b_in=b['POSITION'][0]>=cut
        if a_in:clipped.append(a)
        if a_in!=b_in:
            q=mix(a,b,(cut-a['POSITION'][0])/(b['POSITION'][0]-a['POSITION'][0]));q['POSITION'][0]=cut
            clipped.append(q);boundary.append(q)
    for i in range(1,len(clipped)-1):triangles.append([clipped[0],clipped[i],clipped[i+1]])
unique={tuple(np.round(p['POSITION'],7)):p for p in boundary};ring=list(unique.values())
assert len(ring)>8,'Expected a closed wrist intersection'
center=np.mean([p['POSITION']for p in ring],axis=0)
ring.sort(key=lambda p:np.arctan2(p['POSITION'][2]-center[2],p['POSITION'][1]-center[1]))
middle=mix(ring[0],ring[len(ring)//2],.5);middle['POSITION']=center;middle['NORMAL']=np.array([-1.,0,0])
for a,b in zip(ring,ring[1:]+ring[:1]):
    a={k:v.copy()for k,v in a.items()};b={k:v.copy()for k,v in b.items()}
    a['NORMAL']=b['NORMAL']=np.array([-1.,0,0]);triangles.append([middle,b,a])
right=[v for t in triangles for v in t]
names=[doc['nodes'][i]['name']for i in skin['joints']]
mirror_joint={i:names.index(n.replace('Right','Left')) for i,n in enumerate(names)if n.startswith('Right')}
left=[]
for triangle in triangles:
    for v in [triangle[0],triangle[2],triangle[1]]:
        p={k:a.copy()for k,a in v.items()};p['POSITION'][0]*=-1;p['NORMAL'][0]*=-1
        p['JOINTS_0']=np.array([mirror_joint.get(int(j),int(j))for j in p['JOINTS_0']]);left.append(p)
vertices=right+left
def append(array,kind,component=5126):
    array=np.ascontiguousarray(array);blob.extend(b'\0'*((-len(blob))%4));offset=len(blob);blob.extend(array.tobytes())
    view=len(doc['bufferViews']);doc['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':array.nbytes})
    a={'bufferView':view,'componentType':component,'count':len(array),'type':kind}
    if kind=='VEC3':a.update(min=array.min(axis=0).tolist(),max=array.max(axis=0).tolist())
    result=len(doc['accessors']);doc['accessors'].append(a);return result
prim.pop('indices',None);prim['attributes']={}
for key in attrs:
    values=np.array([v[key]for v in vertices],dtype='<u2'if key=='JOINTS_0'else'<f4')
    prim['attributes'][key]=append(values,{2:'VEC2',3:'VEC3',4:'VEC4'}[values.shape[1]],5123 if key=='JOINTS_0'else 5126)
doc['meshes'][0]['name']='Matched floating hands, sealed wrist cuts'
doc['nodes'][0]['scale']=[1.6,1.6,1.6] # same physical scale applied explicitly by the native pose baker
doc['buffers']=[{'byteLength':len(blob)}];encoded=json.dumps(doc,separators=(',',':')).encode();encoded+=b' '*((-len(encoded))%4);blob.extend(b'\0'*((-len(blob))%4))
(folder/'FloatingHands-WristHUD.glb').write_bytes(struct.pack('<III',0x46546c67,2,28+len(encoded)+len(blob))+struct.pack('<II',len(encoded),0x4e4f534a)+encoded+struct.pack('<II',len(blob),0x004e4942)+blob)
positions=np.array([v['POSITION']for v in vertices]);weights=np.array([v['WEIGHTS_0']for v in vertices])
assert np.all(np.abs(positions[:,0])>=cut-1e-7) and np.allclose(weights.sum(axis=1),1)
assert len(left)==len(right)
report={'cut_at_abs_x_metres':cut,'alignment':'Forearm-side physical edge of wrist HUD','right_vertices':len(right),'left_vertices':len(left),'wrist_sealed':True,'matched_pair':'Left is a mirrored copy of right geometry with remapped left finger joints','retained_grip_clips':len(doc['animations']),'runtime_integration':'implemented; live validation pending','runtime_scale':1.6}
(folder/'floating-hands-fit.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
