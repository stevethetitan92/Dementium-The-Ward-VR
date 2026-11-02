from pathlib import Path
from collections import Counter
import json,numpy as np
from rig_supplied_arms import read_glb,accessor
folder=Path(__file__).resolve().parents[1]/'assets/hands'
doc,blob=read_glb(folder/'FloatingHands-WristHUD.glb');report=json.loads((folder/'floating-hands-fit.json').read_text())
attrs=doc['meshes'][0]['primitives'][0]['attributes'];p=accessor(doc,blob,attrs['POSITION']);n=accessor(doc,blob,attrs['NORMAL'])
w=accessor(doc,blob,attrs['WEIGHTS_0']);j=accessor(doc,blob,attrs['JOINTS_0']);count=report['right_vertices'];cut=report['cut_at_abs_x_metres']
assert len(p)==2*count and np.all(abs(p[:,0])>=cut-1e-7)
assert np.isfinite(p).all() and np.allclose(w.sum(axis=1),1,atol=2e-7)
assert np.allclose(np.linalg.norm(n,axis=1),1,atol=1e-5)
left=p[count:].reshape(-1,3,3)[:,[0,2,1],:].reshape(-1,3).copy();left[:,0]*=-1
assert np.array_equal(left,p[:count]),'Both anatomical hand shapes must match exactly'
names=[doc['nodes'][i]['name']for i in doc['skins'][0]['joints']]
for begin,end,side in [(0,count,'Right'),(count,len(p),'Left')]:
    used=set(j[begin:end][w[begin:end]>1e-7].flatten())
    assert all(names[i].startswith(side)for i in used)
edges=Counter()
for t in p.reshape(-1,3,3):
    for a,b in [(0,1),(1,2),(2,0)]:
        x=tuple(np.round(t[a],6));y=tuple(np.round(t[b],6))
        if x!=y:edges[tuple(sorted((x,y)))]+=1
assert set(edges.values())=={2},'Actual exported wrist caps must be closed and manifold'
assert len(doc['animations'])==8
for side,sign in [('Right',1),('Left',-1)]:
    band=next(n for n in doc['nodes']if n.get('name')==side+' fitted wrist HUD')
    bp=np.concatenate([accessor(doc,blob,q['attributes']['POSITION'])for q in doc['meshes'][band['mesh']]['primitives']])
    assert abs((bp[:,0]*sign+.350).min()-cut)<1e-7
print('PASS: exact mirrored pair, closed manifold wrist caps, flush HUD edges, correct left/right skin joints, normalized weights and eight retained clips')
