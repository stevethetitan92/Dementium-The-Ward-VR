"""Convert exactly rigid skin triangles to animated joint children, preserving motion."""
import copy,json,struct,sys
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'work/psx-right-grips'))
from glb_data import GLB

def partition(source):
 g=GLB(source);doc=copy.deepcopy(g.g);binary=bytearray(g.buf);report=[]
 if len(doc.get('buffers',[]))!=1 or 'uri' in doc['buffers'][0]:raise ValueError('Requires embedded buffer')
 if any('sparse' in a for a in doc['accessors']):raise ValueError('Sparse attributes unsupported')
 for ni,node in enumerate(g.g['nodes']):
  if 'mesh' not in node or 'skin' not in node:continue
  skin=doc['skins'][node['skin']];ib=g.acc(skin['inverseBindMatrices'])
  for pi,p in enumerate(doc['meshes'][node['mesh']]['primitives']):
   a=p['attributes']
   if p.get('mode',4)!=4 or p.get('targets') or 'JOINTS_1' in a or 'WEIGHTS_1' in a:raise ValueError('Unsupported triangle deformation')
   j=g.acc(a['JOINTS_0']).astype(int);w=g.acc(a['WEIGHTS_0']).astype(float);wa=doc['accessors'][a['WEIGHTS_0']]
   if wa.get('normalized'):w/={5121:255,5123:65535}[wa['componentType']]
   if not np.isfinite(w).all() or np.any(w<0) or j.min()<0 or j.max()>=len(skin['joints']):raise ValueError('Invalid weights/joints')
   weights=np.zeros((len(w),len(skin['joints'])))
   for slot in range(4):np.add.at(weights,(np.arange(len(w)),j[:,slot]),w[:,slot])
   rigid=(np.count_nonzero(weights,axis=1)==1)&(weights.max(1)==1);bone=weights.argmax(1)
   faces=g.acc(p['indices']).reshape(-1,3)
   if not np.all(rigid[faces]) or np.any(bone[faces].min(1)!=bone[faces].max(1)):raise ValueError('Blended or cross-joint triangle; cannot partition losslessly')
   for k in np.unique(bone[faces[:,0]]):
    face_ids=np.flatnonzero(bone[faces[:,0]]==k);triangles=faces[face_ids].astype('<u4')
    binary.extend(b'\0'*(-len(binary)%4));offset=len(binary);data=triangles.tobytes();binary.extend(data)
    vi=len(doc['bufferViews']);doc['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':len(data),'target':34963})
    ai=len(doc['accessors']);doc['accessors'].append({'bufferView':vi,'componentType':5125,'count':int(triangles.size),'type':'SCALAR'})
    primitive=copy.deepcopy(p);primitive['indices']=ai;primitive['attributes'].pop('JOINTS_0');primitive['attributes'].pop('WEIGHTS_0')
    mi=len(doc['meshes']);doc['meshes'].append({'primitives':[primitive]})
    child=len(doc['nodes']);joint=skin['joints'][int(k)]
    doc['nodes'].append({'name':f'rigid_{ni}_{pi}_joint_{joint}','mesh':mi,'matrix':ib[k].astype(float).tolist()})
    doc['nodes'][joint].setdefault('children',[]).append(child)
    report.append({'source_node':ni,'source_primitive':pi,'joint':joint,'joint_slot':int(k),'new_node':child,'source_triangle_indices':face_ids.tolist()})
  doc['nodes'][ni].pop('mesh');doc['nodes'][ni].pop('skin')
 if not report:raise ValueError('No skinned primitives')
 doc['buffers'][0]['byteLength']=len(binary);binary.extend(b'\0'*(-len(binary)%4))
 text=json.dumps(doc,separators=(',',':')).encode();text+=b' '*(-len(text)%4)
 payload=struct.pack('<II',len(text),0x4e4f534a)+text+struct.pack('<II',len(binary),0x004e4942)+binary
 return struct.pack('<III',0x46546c67,2,len(payload)+12)+payload,report
