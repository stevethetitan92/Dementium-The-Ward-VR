"""Select mesh instances while preserving every rig node, skin and animation.

This is staging, not runtime export. Excluded mesh data remains in the binary
for reversibility, but is not instantiated even by exporters enumerating nodes.
"""
import argparse, copy, hashlib, json, struct
from pathlib import Path

def select(data,selected):
 if len(data)<20 or struct.unpack_from('<3I',data)!=(0x46546c67,2,len(data)):
  raise ValueError('Invalid GLB header')
 chunks=[];offset=12
 while offset<len(data):
  if offset+8>len(data):raise ValueError('Truncated chunk header')
  size,kind=struct.unpack_from('<2I',data,offset);offset+=8
  if size%4 or offset+size>len(data):raise ValueError('Invalid chunk size')
  chunks.append((kind,data[offset:offset+size]));offset+=size
 if not chunks or chunks[0][0]!=0x4e4f534a:raise ValueError('Missing JSON')
 doc=json.loads(chunks[0][1]);nodes=doc.get('nodes',[])
 if not doc.get('skins'):raise ValueError('Use static staging for unskinned models')
 if not selected or len(set(selected))!=len(selected):raise ValueError('Empty or duplicate selection')
 for i in selected:
  if type(i)!=int or not 0<=i<len(nodes) or 'mesh' not in nodes[i]:raise ValueError('Select actual mesh instance nodes')
 for group in ('buffers','images'):
  if any('uri' in x and not x['uri'].startswith('data:') for x in doc.get(group,[])):raise ValueError('External resources unsupported')
 for skin in doc['skins']:
  if any(type(i)!=int or not 0<=i<len(nodes) for i in skin['joints']):raise ValueError('Invalid joint reference')
 original=copy.deepcopy(doc);excluded=[]
 for i,n in enumerate(nodes):
  if 'mesh' in n and i not in selected:
   excluded.append(i);n.pop('mesh');n.pop('skin',None)
 for key in original:
  if key!='nodes':assert original[key]==doc[key]
 for a,b in zip(original['nodes'],nodes):
  assert {k:v for k,v in a.items() if k not in ('mesh','skin')}=={k:v for k,v in b.items() if k not in ('mesh','skin')}
 raw=json.dumps(doc,separators=(',',':')).encode();raw+=b' '*(-len(raw)%4)
 chunks[0]=(0x4e4f534a,raw)
 payload=b''.join(struct.pack('<2I',len(raw),kind)+raw for kind,raw in chunks)
 return struct.pack('<3I',0x46546c67,2,len(payload)+12)+payload,excluded

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('source',type=Path);p.add_argument('output',type=Path);p.add_argument('--nodes',nargs='+',type=int,required=True);a=p.parse_args()
 data=a.source.read_bytes();result,excluded=select(data,a.nodes)
 with a.output.open('xb') as f:f.write(result)
 print(json.dumps({'source_sha256':hashlib.sha256(data).hexdigest(),'staged_sha256':hashlib.sha256(result).hexdigest(),'selected':a.nodes,'excluded_mesh_instances':excluded,'rigs_preserved':True,'installed':False}))
