"""Lossless embedded glTF material staging. No renderer conversion or installation."""
import argparse,hashlib,json,struct
from pathlib import Path

def digest(data):return hashlib.sha256(data).hexdigest()

def stage(source,output):
 raw=source.read_bytes()
 if len(raw)<20 or struct.unpack_from('<III',raw)!=(0x46546c67,2,len(raw)):raise ValueError('Invalid GLB header')
 chunks=[];offset=12
 while offset<len(raw):
  if offset+8>len(raw):raise ValueError('Truncated chunk')
  size,kind=struct.unpack_from('<II',raw,offset);offset+=8
  if size%4 or offset+size>len(raw):raise ValueError('Invalid chunk length')
  chunks.append((kind,raw[offset:offset+size]));offset+=size
 if not chunks or chunks[0][0]!=0x4e4f534a:raise ValueError('Missing JSON')
 doc=json.loads(chunks[0][1]);bins=[data for kind,data in chunks if kind==0x004e4942]
 if len(bins)!=1 or len(doc.get('buffers',[]))!=1 or 'uri' in doc['buffers'][0]:raise ValueError('Expected single embedded buffer')
 binary=bins[0];payloads={};images=[]
 for i,item in enumerate(doc.get('images',[])):
  if 'uri' in item or 'bufferView' not in item:raise ValueError('Only embedded images supported')
  view=doc['bufferViews'][item['bufferView']];start=view.get('byteOffset',0);end=start+view['byteLength']
  if view.get('buffer',0)!=0 or start<0 or end>len(binary):raise ValueError('Invalid image view')
  data=binary[start:end];sha=digest(data)
  mime=item.get('mimeType');ext={'image/png':'.png','image/jpeg':'.jpg'}.get(mime)
  if ext is None:raise ValueError('Unsupported image MIME '+str(mime))
  if mime=='image/png' and not data.startswith(b'\x89PNG\r\n\x1a\n'):raise ValueError('PNG signature mismatch')
  if mime=='image/jpeg' and not data.startswith(b'\xff\xd8'):raise ValueError('JPEG signature mismatch')
  name='textures/'+sha+ext;payloads[name]=data
  images.append({'source_index':i,'path':name,'mime_type':mime,'sha256':sha,'byte_length':len(data)})
 bindings=[]
 for ni,node in enumerate(doc.get('nodes',[])):
  if 'mesh' not in node:continue
  for pi,primitive in enumerate(doc['meshes'][node['mesh']]['primitives']):
   material=primitive.get('material')
   if material is not None and not 0<=material<len(doc.get('materials',[])):raise ValueError('Invalid material index')
   bindings.append({'node':ni,'mesh':node['mesh'],'primitive':pi,'material':material,
    'attributes':primitive['attributes'],'skin':node.get('skin')})
 catalog={'schema':1,'source_sha256':digest(raw),'source_file':source.name,
  'images':images,'textures':doc.get('textures',[]),'samplers':doc.get('samplers',[]),
  'materials':doc.get('materials',[]),'primitive_bindings':bindings,
  'extensions_used':doc.get('extensionsUsed',[]),'extensions_required':doc.get('extensionsRequired',[]),
  'policy':'Exact source definitions; no PBR baking, alpha removal, color conversion or renderer acceptance',
  'runtime_verified':False}
 # Validate completely before creating a new, non-overwriting destination.
 output.mkdir(parents=True,exist_ok=False)
 (output/'textures').mkdir()
 for name,data in payloads.items():(output/name).write_bytes(data)
 (output/'materials.json').write_text(json.dumps(catalog,indent=2),encoding='utf-8')
 assert source.read_bytes()==raw
 return catalog

if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('source',type=Path);parser.add_argument('output',type=Path);args=parser.parse_args()
 result=stage(args.source,args.output)
 print(json.dumps({'source_sha256':result['source_sha256'],'images':len(result['images']),'materials':len(result['materials']),'bindings':len(result['primitive_bindings'])}))
