"""Check the delivered fitted-band GLB, including its actual joint parenting."""
from pathlib import Path
import json,numpy as np
from rig_supplied_arms import read_glb,accessor
folder=Path(__file__).resolve().parents[1]/'assets/hands'
doc,blob=read_glb(folder/'ReplacementArms-WristHUD.glb')
fit=json.loads((folder/'replacement-wrist-fit.json').read_text())
for side in ['Left','Right']:
    index=next(i for i,n in enumerate(doc['nodes'])if n.get('name')==side+' fitted wrist HUD')
    assert index in doc['nodes'][fit[side]['wrist_node']]['children']
    mesh=doc['meshes'][doc['nodes'][index]['mesh']]
    assert len(mesh['primitives'])==2
    for prim in mesh['primitives']:
        p=accessor(doc,blob,prim['attributes']['POSITION']);n=accessor(doc,blob,prim['attributes']['NORMAL'])
        assert len(p)%3==0 and np.isfinite(p).all() and abs(p).max()<.06
        assert np.allclose(np.linalg.norm(n,axis=1),1,atol=1e-5)
    assert fit[side]['conservative_clearance_m']>.0005
    # Rigid joint-local attachment guarantees the same wrist transform at every pose.
    node=doc['nodes'][index];assert 'skin' not in node and 'matrix' not in node and 'translation' not in node
assert len(doc['animations'])==8
print('PASS: both fitted wrists, joint attachment, mesh bounds, normals, >0.5 mm conservative source-pose clearance, eight retained grip clips')
