"""Read-only, hash-bound replacement profile validation. Never deploys assets.

Node bindings refer to one exact staged asset, not a filename or guessed mesh name.
Passing structural validation does not constitute visual or headset acceptance.
"""
import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

SLOTS={'revolver':0,'pistol':1,'m16':2,'shotgun':3,'hunting-rifle':4,'buzzsaw':5,'nightstick':6,'flashlight':None}
REQUIRED=('part_identity','model_alignment','primary_grip','muzzle_effects','runtime_export','desktop_stereo')

def asset(path):
    data=path.read_bytes()
    if len(data)<20 or struct.unpack_from('<3I',data)!=(0x46546C67,2,len(data)):
        raise ValueError('Invalid GLB: '+str(path))
    size,kind=struct.unpack_from('<2I',data,12)
    if kind!=0x4E4F534A or size+20>len(data):raise ValueError('Invalid JSON chunk')
    return hashlib.sha256(data).hexdigest(),json.loads(data[20:20+size])

def validate(profile,folder):
    errors=[]; pending=[]; docs={}
    weapon=profile.get('weapon')
    if profile.get('schema')!=1:errors.append('Unsupported profile schema')
    if weapon not in SLOTS or profile.get('runtime_slot')!=SLOTS.get(weapon):errors.append('Weapon/runtime slot mismatch')
    for role in ('source','staged'):
        ref=profile.get(role)
        if not ref:
            pending.append(role+' asset is not staged');continue
        try:
            digest,doc=asset(folder/ref['path'])
            if digest!=ref.get('sha256'):errors.append(role+' hash mismatch; bindings must be re-audited')
            docs[role]=doc
        except (OSError,ValueError,KeyError) as e:errors.append(role+': '+str(e))
    doc=docs.get('staged',{})
    catalog_ref=profile.get('material_catalog')
    if not catalog_ref:pending.append('Material catalog not staged')
    else:
        try:
            catalog_path=folder/catalog_ref['path']
            catalog=json.loads(catalog_path.read_text())
            expected=profile.get('staged',{}).get('sha256')
            if catalog.get('source_sha256')!=expected or catalog_ref.get('source_sha256')!=expected:
                errors.append('Material catalog belongs to a different staged asset')
            for key in ('materials','textures','samplers'):
                if catalog.get(key)!=doc.get(key,[]):errors.append('Material catalog differs from staged '+key)
            images=catalog.get('images',[])
            if len(images)!=len(doc.get('images',[])) or [i.get('source_index') for i in images]!=list(range(len(images))):
                errors.append('Material catalog image coverage mismatch')
            for image in images:
                image_path=(catalog_path.parent/image['path']).resolve()
                if not image_path.is_relative_to(catalog_path.parent.resolve()):
                    errors.append('Material catalog texture path escapes its folder');continue
                data=image_path.read_bytes()
                if len(data)!=image.get('byte_length') or hashlib.sha256(data).hexdigest()!=image.get('sha256'):
                    errors.append('Material catalog texture content mismatch')
            expected_bindings=[(n,ni['mesh'],p,primitive.get('material')) for n,ni in enumerate(doc.get('nodes',[])) if 'mesh' in ni
                               for p,primitive in enumerate(doc['meshes'][ni['mesh']]['primitives'])]
            actual=[(b.get('node'),b.get('mesh'),b.get('primitive'),b.get('material')) for b in catalog.get('primitive_bindings',[])]
            if actual!=expected_bindings:errors.append('Material catalog primitive bindings mismatch')
        except (OSError,ValueError,KeyError,TypeError) as e:errors.append('Material catalog: '+str(e))
    bindings=profile.get('bindings',{})
    if not bindings:pending.append('No model parts have been bound')
    for label,binding in bindings.items():
        ids=binding.get('nodes',[])
        if not ids or len(set(ids))!=len(ids):errors.append(label+': empty or duplicate binding')
        for i in ids:
            if not isinstance(i,int) or isinstance(i,bool) or not 0<=i<len(doc.get('nodes',[])):
                errors.append(label+': invalid staged node index')
        if binding.get('status')!='verified':pending.append(label+' identity or motion not verified')
        elif not binding.get('evidence') or not all((folder/p).is_file() for p in binding['evidence']):
            errors.append(label+': verified binding lacks accessible evidence')
    matrix=profile.get('model_to_editor')
    if matrix is None:pending.append('Model alignment is uncalibrated')
    elif not isinstance(matrix,list) or len(matrix)!=16 or not all(type(v) in (float,int) and math.isfinite(v) for v in matrix):
        errors.append('Model alignment must be a finite 4x4 matrix')
    else:
        # glTF column-major convention; permit rotation, translation, uniform
        # positive scale. Reject reflection/shear that silently reverses fits.
        if profile.get('matrix_convention')!='column-major':errors.append('Declare column-major matrix convention')
        if any(abs(matrix[i])>1e-7 for i in (3,7,11)) or abs(matrix[15]-1)>1e-7:
            errors.append('Model alignment must be affine')
        cols=[matrix[i:i+3] for i in (0,4,8)]
        lengths=[math.sqrt(sum(x*x for x in c)) for c in cols]
        if min(lengths)<1e-8:errors.append('Model alignment has zero scale')
        else:
            if max(lengths)-min(lengths)>1e-5*max(lengths):errors.append('Model alignment must use uniform scale')
            if any(abs(sum(cols[a][i]*cols[b][i] for i in range(3)))>1e-5*lengths[a]*lengths[b] for a,b in ((0,1),(0,2),(1,2))):
                errors.append('Model alignment contains shear')
            a,b,c=cols
            det=a[0]*(b[1]*c[2]-b[2]*c[1])-b[0]*(a[1]*c[2]-a[2]*c[1])+c[0]*(a[1]*b[2]-a[2]*b[1])
            if det<=0:errors.append('Model alignment reverses handedness')
    mode=profile.get('runtime_export_mode')
    if mode in (None,'unselected'):pending.append('Runtime export mode is unselected')
    elif mode not in ('static','rigged'):errors.append('Unknown runtime export mode')
    if mode=='static' and (doc.get('skins') or doc.get('animations')):
        errors.append('Static export would discard rig/animation behavior')
    reviews=profile.get('reviews',{})
    for key in REQUIRED+tuple(profile.get('additional_required_reviews',[])):
        review=reviews.get(key,{})
        if review.get('status')!='passed':pending.append(key+' review pending')
        elif not review.get('evidence') or not all((folder/p).is_file() for p in review['evidence']):
            errors.append(key+': passed review lacks accessible evidence')
    return {'weapon':weapon,'structurally_valid':not errors,'errors':errors,'pending':pending,
            'ready_for_install_review':not errors and not pending,'installed':False,
            'physical_headset_acceptance':profile.get('headset_tested',False),
            'note':'Evidence presence is checked; its conclusions still require review. This tool never installs.'}

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('profile',type=Path)
    a=p.parse_args();result=validate(json.loads(a.profile.read_text()),a.profile.parent)
    print(json.dumps(result,indent=2))
    raise SystemExit(0 if result['structurally_valid'] else 1)
