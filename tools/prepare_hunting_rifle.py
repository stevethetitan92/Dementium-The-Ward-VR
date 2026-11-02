"""Convert supplied rifle into separate, metre-scaled native triangle streams.
Retains source texture color at vertices; never joins bolt/trigger/ammo to the stock.
"""
from pathlib import Path
import hashlib,io,json,struct
import numpy as np
from PIL import Image
from rig_supplied_arms import read_glb,accessor

def prepare(source,destination):
    doc,blob=read_glb(source);destination=Path(destination);destination.mkdir(parents=True,exist_ok=True)
    parents={c:i for i,n in enumerate(doc['nodes'])for c in n.get('children',[])};world={}
    def matrix(i):
        if i in world:return world[i]
        node=doc['nodes'][i]
        if 'matrix'in node:m=np.array(node['matrix']).reshape(4,4).T
        else:
            x,y,z,w=node.get('rotation',[0,0,0,1]);m=np.eye(4)
            m[:3,:3]=np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])@np.diag(node.get('scale',[1,1,1]));m[:3,3]=node.get('translation',[0,0,0])
        world[i]=matrix(parents[i])@m if i in parents else m;return world[i]
    parts=[];all_positions=[]
    for i,node in enumerate(doc['nodes']):
        if 'mesh'not in node:continue
        for primitive in doc['meshes'][node['mesh']]['primitives']:
            assert primitive.get('mode',4)==4
            p=accessor(doc,blob,primitive['attributes']['POSITION']);n=accessor(doc,blob,primitive['attributes']['NORMAL']);uv=accessor(doc,blob,primitive['attributes']['TEXCOORD_0'])
            transform=matrix(i);p=(transform@np.c_[p,np.ones(len(p))].T).T[:,:3]
            n=(np.linalg.inv(transform[:3,:3]).T@n.T).T;n/=np.maximum(np.linalg.norm(n,axis=1)[:,None],1e-8)
            order=accessor(doc,blob,primitive['indices']).flatten();assert len(order)%3==0
            material=doc['materials'][primitive['material']]['pbrMetallicRoughness'];texture=doc['textures'][material['baseColorTexture']['index']];image=doc['images'][texture['source']];view=doc['bufferViews'][image['bufferView']]
            im=np.asarray(Image.open(io.BytesIO(blob[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']])).convert('RGB'))/255.
            xy=np.floor((uv%1)*[im.shape[1],im.shape[0]]).astype(int);colors=im[xy[:,1],xy[:,0]]
            parts.append({'name':doc['meshes'][node['mesh']]['name'],'p':p,'n':n,'uv':uv,'colors':colors,'order':order});all_positions.append(p)
    # World model is centimetre-like despite FBX root conversions. Explicitly set a
    # 1.10 m rifle length rather than inheriting a 1,000% weapon size in VR.
    body=parts[0]['p'];span=np.ptp(body,axis=0);long_axis=int(np.argmax(span));scale=1.10/span[long_axis]
    # Canonical native body axes: +X muzzle, Y width, +Z down. Source world +Y muzzle.
    axis=np.zeros((3,3));axis[0,long_axis]=-1 # source muzzle is at negative long-axis end
    remaining=[a for a in range(3)if a!=long_axis];axis[1,remaining[0]]=1;axis[2,remaining[1]]=-1
    if np.linalg.det(axis)<0:axis[1]*=-1
    transformed=body@axis.T*scale
    # Keep all components in one common frame; individual pivots are reported below.
    origin=(transformed.min(axis=0)+transformed.max(axis=0))*.5
    report={'source_sha256':hashlib.sha256(Path(source).read_bytes()).hexdigest(),'units':'metres','length_m':1.10,'source_to_body_rotation':axis.tolist(),'source_scale':float(scale),'source_origin_body':origin.tolist(),'parts':[],
            'status':'Native replacement, separate mechanism motion and shot feedback implemented; live contact and engine validation pending',
            'donor_animation':'Not retargeted: donor has a magazine, but this supplied rifle exposes loose cartridges; magazine motion must not be copied blindly'}
    for part in parts:
        p=part['p']@axis.T*scale-origin;n=part['n']@axis.T;order=part['order']
        data=np.c_[p,n,part['colors'],part['uv'],np.full(len(p),4)].astype('<f4')[order]
        # Reverse winding if an imported hierarchy was reflected.
        name=part['name'].replace('_Rifle_0','').replace('_Bullet_0','');file=name+'.mesh';(destination/file).write_bytes(data.tobytes())
        report['parts'].append({'name':name,'file':file,'vertices':len(data),'bounds':[p.min(axis=0).tolist(),p.max(axis=0).tolist()],'pivot':p.mean(axis=0).tolist()})
        assert np.isfinite(data).all()
    (destination/'model.json').write_text(json.dumps(report,indent=2));print(json.dumps({'parts':len(parts),'length_m':1.10,'triangles':sum(p['vertices']//3 for p in report['parts'])}))

if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser();parser.add_argument('source');parser.add_argument('destination');args=parser.parse_args();prepare(args.source,args.destination)
