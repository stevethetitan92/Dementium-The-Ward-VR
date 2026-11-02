"""Offline, repeatable glTF skin/pose authoring for the supplied unrigged arms.
No Blender, game process, network, or runtime injection required.
"""
import argparse, copy, hashlib, json, struct
from pathlib import Path
import numpy as np

def read_glb(path):
    data=Path(path).read_bytes()
    magic,version,length=struct.unpack_from('<III',data)
    assert magic==0x46546c67 and version==2 and length==len(data)
    n,kind=struct.unpack_from('<II',data,12); assert kind==0x4e4f534a
    doc=json.loads(data[20:20+n]); size,kind=struct.unpack_from('<II',data,20+n)
    assert kind==0x004e4942
    return doc,bytearray(data[28+n:28+n+size])

def accessor(doc,blob,index):
    a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']]
    dtype={5126:'<f4',5123:'<u2',5125:'<u4',5121:'u1'}[a['componentType']]
    width={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}[a['type']]
    return np.ndarray((a['count'],width),dtype=dtype,buffer=blob,
        offset=v.get('byteOffset',0)+a.get('byteOffset',0),
        strides=(v.get('byteStride',np.dtype(dtype).itemsize*width),np.dtype(dtype).itemsize)).copy()

def build(source,destination):
    doc,blob=read_glb(source)
    primitive=copy.deepcopy(doc['meshes'][1]['primitives'][0]) # The supplied open T-pose pair.
    positions=accessor(doc,blob,primitive['attributes']['POSITION'])*.1
    normals=accessor(doc,blob,primitive['attributes']['NORMAL'])
    nodes=[{'name':'Replacement arms','children':[1,2]},
           {'name':'Arms mesh','mesh':0,'skin':0}, {'name':'Hand skeleton','children':[]}]
    joints=[];origins=[];segments=[];chains=[];side_ranges={}
    def joint(name,p,parent,end,chain):
        index=len(nodes); p=np.array(p,float)
        parent_origin=np.zeros(3) if parent==2 else origins[joints.index(parent)]
        nodes.append({'name':name,'translation':(p-parent_origin).tolist(),'children':[]})
        nodes[parent].setdefault('children',[]).append(index)
        joints.append(index);origins.append(p);segments.append((p,np.array(end,float)));chains.append(chain)
        return index
    for side,sign in [('Right',1),('Left',-1)]:
        start=len(joints)
        shoulder=joint(side+'Shoulder',[sign*.09,-.005,0],2,[sign*.22,-.02,0],side+'Arm')
        elbow=joint(side+'Forearm',[sign*.22,-.02,0],shoulder,[sign*.35,-.02,0],side+'Arm')
        wrist=joint(side+'Hand',[sign*.35,-.020,0],elbow,[sign*.385,-.020,0],side+'Palm')
        # Landmarks in the actual supplied open mesh; metres after source unit conversion.
        for finger,z,length in [('Index',-.018,.049),('Middle',-.002,.058),('Ring',.014,.052),('Pinky',.028,.040)]:
            parent=wrist
            for k in range(3):
                p=[sign*(.380+length*k/3),-.020,z]
                end=[sign*(.380+length*(k+1)/3),-.020,z]
                parent=joint(side+finger+str(k+1),p,parent,end,side+finger)
        parent=wrist
        for k in range(3):
            p=[sign*(.365+.018*k/3),-.018,-.020-.026*k/3]
            end=[sign*(.365+.018*(k+1)/3),-.018,-.020-.026*(k+1)/3]
            parent=joint(side+'Thumb'+str(k+1),p,parent,end,side+'Thumb')
        side_ranges[side]=(start,len(joints))
    weights=np.zeros((len(positions),4),np.float32);indices=np.zeros((len(positions),4),np.uint16)
    for row,p in enumerate(positions):
        side='Right' if p[0]>=0 else 'Left'; lo,hi=side_ranges[side]
        distances=[]
        for i in range(lo,hi):
            a,b=segments[i];d=b-a;t=np.clip(np.dot(p-a,d)/np.dot(d,d),0,1)
            distances.append((float(np.linalg.norm(p-a-t*d)),i))
        distances.sort(); nearest=distances[0][1]
        # Do not blend adjacent fingers together across the gaps.
        candidates=[v for v in distances if chains[v[1]]==chains[nearest] or
                    ('Palm' in chains[v[1]] and abs(p[0])<.395)]
        chosen=candidates[:4];raw=np.array([1/(d+.002)**4 for d,i in chosen]);raw/=raw.sum()
        for slot,((d,i),w) in enumerate(zip(chosen,raw)):indices[row,slot]=i;weights[row,slot]=w
    # Keep the complete wristband span rigid with the hand. Finger/thumb weights
    # ramp in beyond the cuff, preventing flexion from pulling skin out of it.
    for row,p in enumerate(positions):
        if abs(p[0])<.330:continue
        side='Right'if p[0]>=0 else'Left';wrist_joint=side_ranges[side][0]+2
        # Thumb web starts beside the palm, earlier than the four finger knuckles.
        thumb_zone=float(np.clip((-p[2]-.018)/.014,0,1))
        finger_blend=float(np.clip((abs(p[0])-.370)/.010,0,1))
        thumb_blend=float(np.clip((abs(p[0])-.353)/.014,0,1))*thumb_zone
        blend=max(finger_blend,thumb_blend);combined={wrist_joint:1-blend}
        for j,w in zip(indices[row],weights[row]):combined[int(j)]=combined.get(int(j),0)+w*blend
        chosen=sorted(combined.items(),key=lambda item:-item[1])[:4];chosen += [(0,0)]*(4-len(chosen))
        indices[row]=[j for j,w in chosen];raw=np.array([w for j,w in chosen]);weights[row]=raw/raw.sum()
    def append(array,kind,component=5126):
        array=np.ascontiguousarray(array);blob.extend(b'\0'*((-len(blob))%4));offset=len(blob);blob.extend(array.tobytes())
        view=len(doc['bufferViews']);doc['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':array.nbytes})
        a={'bufferView':view,'componentType':component,'count':len(array),'type':kind}
        if kind in ('VEC3','SCALAR'):a.update(min=array.min(axis=0).tolist(),max=array.max(axis=0).tolist())
        result=len(doc['accessors']);doc['accessors'].append(a);return result
    primitive['attributes']={k:v for k,v in primitive['attributes'].items() if k in ('POSITION','NORMAL','TEXCOORD_0')}
    primitive['attributes']['POSITION']=append(positions.astype('<f4'),'VEC3')
    primitive['attributes']['JOINTS_0']=append(indices,'VEC4',5123)
    primitive['attributes']['WEIGHTS_0']=append(weights,'VEC4')
    inverse=[]
    for p in origins:
        m=np.eye(4,dtype=np.float32);m[:3,3]=-p;inverse.append(m.T.reshape(16))
    doc['skins']=[{'name':'Matched articulated hands','skeleton':2,'joints':joints,'inverseBindMatrices':append(np.array(inverse,'<f4'),'MAT4')}]
    doc['nodes']=nodes;doc['meshes']=[{'name':'Supplied arms, rigged open pose','primitives':[primitive]}]
    doc['scenes']=[{'nodes':[0]}];doc['scene']=0;doc.pop('cameras',None);doc['animations']=[]
    # These are authored grip poses, not a claim of retargeted donor reload motion.
    for pose,curls in [('Relaxed',[.2,.25,.25,.3]),('WeaponGrip',[.25,1.3,1.35,1.35]),
                       ('TriggerGrip',[1.05,1.3,1.35,1.35]),('PillBottleGrip',[1.25,1.35,1.4,1.4]),
                       ('MagazineGrip',[.6,.85,.9,.95]),('ShellPinch',[.7,.3,.65,.7]),
                       ('NightstickGrip',[1.15,1.35,1.4,1.4]),('BuzzsawGrip',[.85,1.2,1.3,1.3])]:
        animation={'name':pose,'samplers':[],'channels':[]};time=append(np.array([[0],[.25]],'<f4'),'SCALAR')
        for index in joints:
            name=nodes[index]['name'];finger=next((f for f in ['Index','Middle','Ring','Pinky','Thumb'] if f in name),None)
            if not finger:continue
            angle=(.08 if finger=='Thumb' else curls[['Index','Middle','Ring','Pinky'].index(finger)])
            if pose=='TriggerGrip' and finger=='Index':angle=[.25,1.05,.50][int(name[-1])-1]
            if name.startswith('Right'):angle=-angle
            q=[0,0,np.sin(angle/2),np.cos(angle/2)]
            if finger=='Thumb' and not pose=='Relaxed':
                sign=-1 if name.startswith('Left')else 1
                if name.endswith('1'):
                    # Oppose the thumb across the palm before flexing it.
                    y=-sign*.45;z=-sign*.18
                    q=[np.sin(y/2)*np.sin(z/2),np.sin(y/2)*np.cos(z/2),np.cos(y/2)*np.sin(z/2),np.cos(y/2)*np.cos(z/2)]
                else:q=[0,0,np.sin(-sign*.30/2),np.cos(.30/2)]
            output=append(np.array([[0,0,0,1],q],'<f4'),'VEC4')
            animation['samplers'].append({'input':time,'output':output,'interpolation':'LINEAR'})
            animation['channels'].append({'sampler':len(animation['samplers'])-1,'target':{'node':index,'path':'rotation'}})
        doc['animations'].append(animation)
    assert np.allclose(weights.sum(axis=1),1) and np.isfinite(weights).all()
    # Rest skin must reproduce the original mesh exactly before any authored grip.
    for i,p in enumerate(origins):
        m=np.eye(4);m[:3,3]=p;assert np.allclose(m@np.array(inverse[i]).reshape(4,4).T,np.eye(4),atol=1e-6)
    doc['buffers']=[{'byteLength':len(blob)}];raw=json.dumps(doc,separators=(',',':')).encode();raw+=b' '*((-len(raw))%4);blob.extend(b'\0'*((-len(blob))%4))
    destination=Path(destination);destination.parent.mkdir(parents=True,exist_ok=True)
    destination.write_bytes(struct.pack('<III',0x46546c67,2,28+len(raw)+len(blob))+struct.pack('<II',len(raw),0x4e4f534a)+raw+struct.pack('<II',len(blob),0x004e4942)+blob)
    report={'source_sha256':hashlib.sha256(Path(source).read_bytes()).hexdigest(),'vertices':len(positions),'joints':len(joints),'poses':[a['name']for a in doc['animations']],
            'weight_sum_max_error':float(abs(weights.sum(axis=1)-1).max()),'status':'Rig and native pose integration implemented; live contact and tracking remain unverified'}
    destination.with_suffix('.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('source');parser.add_argument('destination');a=parser.parse_args();build(a.source,a.destination)
