"""Export independent local-space draws for a staged replacement.

Intermediate package only: keeps hierarchy, animation and material IDs instead of
flattening everything into the installed legacy hand/weapon atlas.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path
import numpy as np
from stage_material_catalog import stage


def export(source, output):
    raw = source.read_bytes()
    if struct.unpack_from('<III', raw) != (0x46546C67, 2, len(raw)):
        raise ValueError('Invalid GLB')
    offset = 12; chunks = []
    while offset < len(raw):
        size, kind = struct.unpack_from('<II', raw, offset); offset += 8
        if size % 4 or offset + size > len(raw): raise ValueError('Invalid chunk')
        chunks.append((kind, raw[offset:offset+size])); offset += size
    if len(chunks) != 2 or [c[0] for c in chunks] != [0x4E4F534A, 0x004E4942]:
        raise ValueError('Expected JSON and embedded BIN')
    doc = json.loads(chunks[0][1]); binary = chunks[1][1]

    def accessor(index):
        a = doc['accessors'][index]
        if 'sparse' in a: raise ValueError('Sparse accessor needs explicit expansion')
        view = doc['bufferViews'][a['bufferView']]
        if view.get('buffer', 0) != 0: raise ValueError('External buffer')
        dtype = np.dtype({5120:'i1',5121:'u1',5122:'<i2',5123:'<u2',5125:'<u4',5126:'<f4'}[a['componentType']])
        width = {'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}[a['type']]
        stride = view.get('byteStride', width*dtype.itemsize)
        start = a.get('byteOffset', 0); count = a['count']
        end = start + max(0,count-1)*stride + (width*dtype.itemsize if count else 0)
        if start < 0 or stride < width*dtype.itemsize or end > view['byteLength']:
            raise ValueError('Accessor exceeds view')
        absolute = view.get('byteOffset',0)+start
        if absolute + end-start > len(binary): raise ValueError('Accessor exceeds BIN')
        values = np.ndarray((count,width), dtype=dtype, buffer=binary, offset=absolute,
                            strides=(stride,dtype.itemsize)).copy()
        if not np.isfinite(values).all(): raise ValueError('Non-finite accessor')
        return values

    # Only the explicitly selected scene is drawable. Orphan display props stay out.
    roots = doc['scenes'][doc.get('scene',0)]['nodes']; active = set(); visiting = set()
    def visit(n):
        if n in visiting or n in active: raise ValueError('Cycle or repeated scene node')
        visiting.add(n)
        for child in doc['nodes'][n].get('children',[]): visit(child)
        visiting.remove(n); active.add(n)
    for root in roots: visit(root)
    arrays = {}; draws = []
    for ni in sorted(active):
        node = doc['nodes'][ni]
        if 'mesh' not in node: continue
        if 'skin' in node: raise ValueError('Convert/validate skin before rigid draw export')
        for pi,p in enumerate(doc['meshes'][node['mesh']]['primitives']):
            if p.get('mode',4)!=4 or p.get('targets') or p.get('extensions'):
                raise ValueError('Unsupported primitive; do not silently flatten')
            positions = accessor(p['attributes']['POSITION'])
            ids = accessor(p['indices']).ravel()
            if ids.dtype.kind != 'u' or len(ids)%3 or (len(ids) and ids.max()>=len(positions)):
                raise ValueError('Invalid triangle indices')
            used, compact = np.unique(ids, return_inverse=True)
            prefix = f'draw_{len(draws)}'; attrs = {}
            arrays[prefix+'_indices'] = compact.astype('<u4').reshape(-1,3)
            for semantic, ai in p['attributes'].items():
                values = accessor(ai)
                if len(values)!=len(positions): raise ValueError('Attribute count mismatch')
                key=prefix+'_'+semantic; arrays[key]=values[used]
                a=doc['accessors'][ai]
                attrs[semantic]={'array':key,'component_type':a['componentType'],
                                 'normalized':a.get('normalized',False),'type':a['type']}
            material = p.get('material')
            m = {} if material is None else doc['materials'][material]
            alpha = m.get('alphaMode','OPAQUE')
            if alpha not in ('OPAQUE','MASK','BLEND'): raise ValueError('Unknown alpha mode')
            draws.append({'node':ni,'source_primitive':pi,'material':material,
                          'attributes':attrs,'indices':prefix+'_indices','vertices':len(used),
                          'triangles':len(ids)//3,'alpha_mode':alpha,
                          'alpha_cutoff':m.get('alphaCutoff',.5),'double_sided':m.get('doubleSided',False),
                          'depth_write':alpha!='BLEND','requires_depth_sort':alpha=='BLEND'})
    animations=[]
    for ci,clip in enumerate(doc.get('animations',[])):
        samplers=[]
        for si,s in enumerate(clip['samplers']):
            mode=s.get('interpolation','LINEAR')
            if mode not in ('LINEAR','STEP','CUBICSPLINE'): raise ValueError('Unknown interpolation')
            entry={'interpolation':mode}
            for kind in ('input','output'):
                key=f'clip_{ci}_sampler_{si}_{kind}'; arrays[key]=accessor(s[kind]);entry[kind]=key
            samplers.append(entry)
        animations.append({'name':clip.get('name'),'channels':clip['channels'],'samplers':samplers})
    manifest={'schema':1,'source_sha256':hashlib.sha256(raw).hexdigest(),'scene_roots':roots,
              'nodes':doc['nodes'],'draws':draws,'animations':animations,
              'coordinate_policy':'Source local coordinates; node hierarchy applied once. Calibration not baked.',
              'runtime_verified':False,'deployment_ready':False}
    # stage validates materials and refuses to overwrite any previous package.
    stage(source,output)
    np.savez(output/'geometry.npz',**arrays)
    manifest['geometry_sha256']=hashlib.sha256((output/'geometry.npz').read_bytes()).hexdigest()
    (output/'draws.json').write_text(json.dumps(manifest,indent=2))
    assert source.read_bytes()==raw
    return manifest

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();m=export(a.source,a.output)
    print(json.dumps({'draws':len(m['draws']),'animations':len(m['animations']),'deployment_ready':False}))
